#include "RegisterJsonData.h"

RegisterJsonData::RegisterJsonData(const std::string& message)
{
    parse(message);
}

void RegisterJsonData::parse(const std::string& message)
{
    if(message.empty())
    {
        return;
    }
    ptree m_ptree;
    std::stringstream sstream(message);
    read_json(sstream,m_ptree);
    std::string userid=m_ptree.get<std::string>("UserId");
    m_strUserId=userid;
    std::string userpassword=m_ptree.get<std::string>("UserPassword");
    m_strUserPassword=userpassword;
    return;
}

std::string RegisterJsonData::generateJson()
{
    ptree m_ptree;
    m_ptree.put("UserId",m_strUserId);
    m_ptree.put("UserPassword",m_strUserPassword);
    std::stringstream sstream;
    write_json(sstream,m_ptree);
    return sstream.str();
}
