/*
 * @Author: qin11152 1052080761@qq.com
 * @Date: 2022-08-13 20:08:37
 * @LastEditors: qin11152 1052080761@qq.com
 * @LastEditTime: 2022-11-27 13:12:29
 * @FilePath: /src/IM-Server/protocol/ProfileImageMsgJsonData.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "ProfileImageMsgJsonData.h"

ProfileImageMsgJsonData::ProfileImageMsgJsonData(const std::string& message/*=""*/)
{
    parse(message);
}

void ProfileImageMsgJsonData::parse(const std::string& message)
{
    if (message.empty())
    {
        return;
    }
    ptree m_ptree;
    std::stringstream sstream(message);
    read_json(sstream, m_ptree);
    m_strId = m_ptree.get<std::string>("UserId");
    m_strUUID = m_ptree.get<std::string>("ImageUUID");
    m_strBase64Msg = m_ptree.get<std::string>("ImageBase64Msg");
    m_strImageName = m_ptree.get<std::string>("ImageName");
    m_strSuffix = m_ptree.get<std::string>("Suffix");
    m_strTimeStamp = m_ptree.get<std::string>("TimeStamp");
    m_iCurIndex = m_ptree.get<int>("ImageIndex");
    m_iSumIndex = m_ptree.get<int>("SumIndex");
}

std::string ProfileImageMsgJsonData::generateJson()
{
    ptree m_ptree;

    m_ptree.put("type",static_cast<int>(m_strType));
    m_ptree.put("UserId",m_strId);
    m_ptree.put("ImageUUID",m_strUUID);
    m_ptree.put("ImageBase64Msg",m_strBase64Msg);
    m_ptree.put("ImageName",m_strImageName);
    m_ptree.put("Suffix",m_strSuffix);
    m_ptree.put("TimeStamp",m_strTimeStamp);
    m_ptree.put("ImageIndex",m_iCurIndex);
    m_ptree.put("SumIndex",m_iSumIndex);

    std::stringstream sstream;
    write_json(sstream,m_ptree);
    return sstream.str();
}