#include "ChatClient.h"
#include "ChatServer.h"
#include "RegisterJsonData.h"
#include "RegisterReplyData.h"
#include "LoginInReplyData.h"
#include "InitialRequestJsonData.h"
#include "SingleChatMessageJsonData.h"
#include "GetFriendListReplyData.h"
#include "AddFriendResponseJsonData.h"
#include "AddFriendRequestJsonData.h"
#include "AddFriendNotifyJsonData.h"
#include "ProfileImageMsgJsonData.h"
#include "GetProfileImageJsonData.h"
#include "../PublicFunction.hpp"
#include "Log.h"
#include <fstream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace boost;

constexpr int kHeartPackageTime=300;
//初始化的时候会把socket move过来，会保存服务器的指针，会定时5分钟的心跳保活
ChatClient::ChatClient(tcp::socket sock,ChatServer* ptr,boost::asio::io_context& ioc)
:m_clientSocket(std::move(sock)),
m_ptrChatServer(ptr),
m_timer(ioc,boost::posix_time::seconds(kHeartPackageTime))
{ 
    //m_timer.async_wait(std::bind(&ChatClient::removeSelfFromServer,this));
    m_timer.async_wait
    (
        [this](const std::error_code& ec)
        {
            //printf("error code is:%d\n",ec.value());
            if(0!=ec.value())
            {
                return;
            }
            m_clientSocket.cancel();
            auto self=shared_from_this();
            m_bReadCancel=true;
            m_ptrChatServer->removeDisconnetedClient(m_iId,self);
        }
    );
}

ChatClient::~ChatClient()
{
    printf("des\n");
    m_cancel=true;
    m_timer.cancel();
    //运行状态标志位false，因为测试发现析构后定时器回调函数还会执行，待改进
    //取消所有异步任务
    m_bReadCancel=true;
    m_clientSocket.cancel();
    m_clientSocket.close();
    //TODO修改这个client对应id的状态
}

void ChatClient::Start()
{
    //开启之后就开始读
    DoRead();
}

void ChatClient::DoWrite(const std::string& message,int length)
{
    //printf("write message:%s\n",message.c_str());
    std::string msgLength=std::to_string(length);
    if(msgLength.length()<PackageHeadSize)
    {
        std::string tmpStr(PackageHeadSize-msgLength.length(),'0');   
        msgLength=tmpStr+msgLength;
    }
    msgLength=msgLength+message;
    int cnt=boost::asio::write(m_clientSocket,boost::asio::buffer(msgLength.c_str(),length+PackageHeadSize));
    //auto self=shared_from_this();
    /*LengthInfo l(length);
    char lengthbuffer[sizeof(LengthInfo)+length]{ 0 };
	memcpy(lengthbuffer, &l, sizeof(LengthInfo));
    memcpy(lengthbuffer+4,&message,length);*/
    /*boost::asio::async_write(m_clientSocket,boost::asio::buffer(message.c_str(),length),
            [this](boost::system::error_code ec){
                if(!ec)
                {
                    printf("send message\n");
                }
            });*/
    /*m_clientSocket.async_write_some(boost::asio::buffer(lengthbuffer,4+length),[this](boost::system::error_code ec){
        if(!ec)
        {
            printf("send message\n");
        }
    });*/
}

void ChatClient::DoRead()
{
    //从自身得到的智能指针，避免异步读取的时候client已经析构了
    auto self=shared_from_this();
    //读取错误的信息
    boost::system::error_code ec;
    //首先清空读缓冲区
    memset(m_oneBuffer,0,FIRSTBUFFERLENGTH);
    //开启异步读任务，传递buffer和回调函数，这里用的lambda表达式
    m_clientSocket.async_read_some
    (
        boost::asio::buffer(m_oneBuffer,FIRSTBUFFERLENGTH),
        [this,self](std::error_code ec,size_t length)
        {
            if(m_bReadCancel==true)
            {
                return;
            }
            //length indicate the lenth of recevied message
                if(!ec)
                {
                    //会把从网络传递来的数据从小缓冲区存在一个大缓冲区中，在大缓冲区中进行业务处理
                    memcpy(m_cBuffer+m_endPosOfBuffer,m_oneBuffer,length);
                    //每次存储到大缓冲区后都要更新他的尾部标识
                    m_endPosOfBuffer+=length;

                    //一直等待，直到读到了足够的字节数
                    if(m_endPosOfBuffer<PackageHeadSize)
                    {
                        //清空一下从底层接受消息的缓冲区
                        memset(m_oneBuffer,0,FIRSTBUFFERLENGTH);
                        //然后继续读取
                        DoRead();
                    }
                    //当数据大于包头长度的时候就要一直去处理
                    while(m_endPosOfBuffer>PackageHeadSize)
                    {
                    //执行到此认为长度是大于包头的，然后去获取长度
                    //char lengthBuf[PackageHeadSize+1]{0};
                    //memcpy(lengthBuf,m_cBuffer,PackageHeadSize);
                    std::string lengthStr(m_cBuffer,PackageHeadSize);
                    //得到数据包的长度
                    int lengthOfMessage=atoi(lengthStr.c_str());
                    //进行第一次业务处理,查看是收到的数据是否大于包头指示的长度
                    //大于的时候就处理，小于就去继续读
                    if(m_endPosOfBuffer>=lengthOfMessage+PackageHeadSize)
                    {
                        //从buffer中取出这个长度的信息
                        //char message[lengthOfMessage+1]{0};
                        //memcpy(message,m_cBuffer+PackageHeadSize,lengthOfMessage);
                        std::string test(m_cBuffer+PackageHeadSize,lengthOfMessage);
                        //因已取出一部分信息，要把大缓冲区的内容更新一下
                        memcpy(m_cBuffer,m_cBuffer+lengthOfMessage+PackageHeadSize,BUFFERLENGTH-lengthOfMessage-PackageHeadSize);
                        //尾部标识也更新一下
                        m_endPosOfBuffer-=(lengthOfMessage+PackageHeadSize);
                        handleClientMessage(test);
                    }
                    else{
                        //小于的时候，包头还是留在缓冲区，然后继续去读
                        //下次再进来还是先处理前面的包头
                        break;
                    }
                    }
                    //业务处理完了，继续读
                    DoRead();
                }
                //读取到了错误或者断开连接
                else
                {
                    //read eof or connection reset by peer
                    //正常断开连接的时候会受到eof,而程序直接关闭，并且不处理socket的时候就会触发104错误
                    if(104==ec.value()||boost::asio::error::eof==ec.value())
                    {
                        printf("client disconnect,code:%d\n",ec.value());
                        //主动断开连接时，关闭socket，socket关闭后会立刻执行定时器的回调函数
                        m_clientSocket.cancel();
                        auto self=shared_from_this();
                        m_bReadCancel=true;
                        m_ptrChatServer->removeDisconnetedClient(m_iId,self);
                    }
                    return;
                }
        }
    );
}

bool ChatClient::praseJsonString(std::string& message,ptree& pt)
{
    //transfer to stream
    std::stringstream ss(message);
    //read json type
    read_json(ss,pt);
    return true;
}

#if 0
//从服务器移除某个连接上的用户
void ChatClient::removeSelfFromServer()
{
    if(m_cancel)
    {
        m_cancel=false;
        return;
    }
    m_clientSocket.cancel();
    auto self=shared_from_this();
    m_bReadCancel=true;
    m_ptrChatServer->removeDisconnetedClient(m_iId,self);
}
#endif

//处理客户端发送来的消息
void ChatClient::handleClientMessage(const std::string& message)
{
    //收到任意消息，都把定时器更新一下
    /*m_cancel=true;
    m_timer.cancel();
    m_timer.expires_at(m_timer.expires_at()+boost::posix_time::seconds(kHeartPackageTime)-m_timer.expires_from_now());
    m_timer.async_wait(std::bind(&ChatClient::removeSelfFromServer,this));*/
    m_timer.cancel();
    m_timer.expires_at(m_timer.expires_at()+boost::posix_time::seconds(kHeartPackageTime)-m_timer.expires_from_now());
    m_timer.async_wait
    (
        [this](const std::error_code& ec)
        {
            //printf("error code is:%d\n",ec.value());
            if(0!=ec.value())
            {
                return;
            }
            m_clientSocket.cancel();
            auto self=shared_from_this();
            m_bReadCancel=true;
            m_ptrChatServer->removeDisconnetedClient(m_iId,self);
        }
    );
    if(""==message)
    {
        return;
    }
    //printf("recv message:%s\n",message.c_str());
    //传递的消息类型为json格式
    //同ptree来解析
    _LOG(Logcxx::INFO,"enter handle clientMessage: %s",message.c_str());
    //不满足通信的格式就不要处理，避免崩溃掉
    if(message.length()<7||message.substr(0,7)!="{\"type\"")
    {
        return;
    }
    ptree pt;
    std::stringstream ss(message);
    read_json(ss,pt);

    //首先获取这次得到的消息的类型
    int imessageType=pt.get<int>("type");
    //_LOG(Logcxx::INFO,"enter handle clientMessage and handle finish type is:%d",imessageType);
    //根据消息的类型做相应的处理
    switch (imessageType)
    {
    //收到注册类型的消息
    case static_cast<int>(MessageType::RegisterRequest):
        {
            std::string userName=pt.get<std::string>("UserName");
            std::string password=pt.get<std::string>("UserPassword");
            bool registerState=false;
            int id=MysqlQuery::Instance()->InsertNewUser(userName,password,"");
            if(-1!=id)
            {
                registerState=true;
            }
            RegisterReplyData registerReplyData;
            registerReplyData.m_bRegisterResult=registerState;
            registerReplyData.m_iId=id;
            std::string replyMessage=registerReplyData.generateJson();
            DoWrite(replyMessage,replyMessage.length());
        }
        break;
    //收到添加朋友类型的消息    
    case static_cast<int>(MessageType::AddFriendRequest):
        {
            AddFriendRequestJsonData addFriendRequestData(message);
            //查看用户是否存在
            if(MysqlQuery::Instance()->queryUserIsExist(addFriendRequestData.m_strFriendId))
            {
                if(MysqlQuery::Instance()->queryUserIsOnline(addFriendRequestData.m_strFriendId))
                {
                    //如果在线，就直接转发消息就行了
                    auto msg=message;
                    m_ptrChatServer->transferMessage(atoi(addFriendRequestData.m_strFriendId.c_str()),msg);

                    //把头像也发送过去
                    auto imagePath=MysqlQuery::Instance()->queryImagePathAcordId(addFriendRequestData.m_strMyId);
                    std::string timeStamp=MysqlQuery::Instance()->queryImageTimeStampAcordId(addFriendRequestData.m_strMyId);
                    std::string suffix=imagePath.substr(imagePath.find_last_of('.') + 1);//获取文件后缀
                    boost::uuids::uuid uid = boost::uuids::random_generator()();
                    const std::string uid_str = boost::uuids::to_string(uid);
                    if(imagePath!="")
                    {
                        std::fstream in(imagePath,std::ios::in);
                        if(in.is_open())
                        {
                            std::string base64Str;
                            in>>base64Str;
                            in.close();
                            int iSumIndex=base64Str.length()/kSegmentLength;
                            if(base64Str.length()%kSegmentLength!=0)
                            {
                                iSumIndex++;
                            }
                            for(int i=0;i<iSumIndex;i++)
                            {
                                ProfileImageMsgJsonData profileImageMsgJsonData;
                                profileImageMsgJsonData.m_strId=addFriendRequestData.m_strMyId;
                                profileImageMsgJsonData.m_strUUID=uid_str;
                                profileImageMsgJsonData.m_strSuffix=suffix;
                                profileImageMsgJsonData.m_strTimeStamp=timeStamp;
                                profileImageMsgJsonData.m_iCurIndex=i+1;
                                profileImageMsgJsonData.m_iSumIndex=iSumIndex;
                                profileImageMsgJsonData.m_strBase64Msg=base64Str.substr(i*kSegmentLength,kSegmentLength);
                                profileImageMsgJsonData.m_eImageType=ProfileImageType::AddFriendProfileImage;
                                auto sendStr=profileImageMsgJsonData.generateJson();
                                m_ptrChatServer->transferMessage(atoi(addFriendRequestData.m_strFriendId.c_str()),sendStr);
                            }
                        }
                        else
                        {
                            _LOG(Logcxx::Level::ERROR,u8"打开文件失败");
                        }
                    }
                    else
                    {
                        _LOG(Logcxx::Level::ERROR,u8"数据库中没有该用户的头像");
                    }
                }
                else
                {
                    //存储在数据库中，上线后推送
                    MysqlQuery::Instance()->insertAddFriendCache(addFriendRequestData.m_strMyId,addFriendRequestData.m_strFriendId,addFriendRequestData.m_strVerifyMsg);
                }
            }
        }
        break;
    //同意或不同意添加好友
    case static_cast<int>(MessageType::AddFriendResponse):
        {
            AddFriendResponseJsonData addFriendResponseData(message);
            //如果同意，双方的好友库里增加好友信息
            if(addFriendResponseData.m_bResult)
            {
                //通知双方你们已经是好友了
                AddFriendNotify addFriendNotifyData;
                addFriendNotifyData.m_strId1=addFriendResponseData.m_strFriendId;
                addFriendNotifyData.m_strId2=addFriendResponseData.m_strMyId;
                addFriendNotifyData.m_strName1=MysqlQuery::Instance()->queryUserNameAcordId(addFriendResponseData.m_strFriendId);
                addFriendNotifyData.m_strName2=MysqlQuery::Instance()->queryUserNameAcordId(addFriendResponseData.m_strMyId);
                addFriendNotifyData.m_strImageStamp1=MysqlQuery::Instance()->queryImageTimeStampAcordId(addFriendResponseData.m_strFriendId);
                addFriendNotifyData.m_strImageStamp2=MysqlQuery::Instance()->queryImageTimeStampAcordId(addFriendResponseData.m_strMyId);
                auto sendStr=addFriendNotifyData.generateJson();
                //通知到好友
                //查看好友是否在线，在线就通知
                if(MysqlQuery::Instance()->queryUserIsOnline(addFriendResponseData.m_strFriendId))
                {
                    m_ptrChatServer->transferMessage(atoi(addFriendResponseData.m_strFriendId.c_str()),sendStr);
                }
                //通知到自己
                DoWrite(sendStr,sendStr.length());
                MysqlQuery::Instance()->AddFriend(addFriendResponseData.m_strFriendId,addFriendResponseData.m_strMyId,addFriendNotifyData.m_strName1,addFriendNotifyData.m_strName2);
            }
            return;
        }
        break;
    //登录请求
    case static_cast<int>(MessageType::LoginRequest):
        {
            std::string userId=pt.get<std::string>("UserId");
            std::string password=pt.get<std::string>("UserPassword");
            bool logInState=false;
            //查询一下数据库
            if(MysqlQuery::Instance()->VertifyPassword(atoi(userId.c_str()),password))
            {
                logInState=true;
            }
            LoginInReplyData logInReplyData;
            logInReplyData.m_strUserName=MysqlQuery::Instance()->queryUserNameAcordId(userId);
            logInReplyData.m_bLoginInResult=logInState;
            std::string loginReplyMessage=logInReplyData.generateJson();
            DoWrite(loginReplyMessage,loginReplyMessage.length());
            }
        break;
    //客户端发来的心跳包
    case static_cast<int>(MessageType::HeartPackage):
        {
        }
        break;
    //用户第一次登录上发来的初始化消息
    case static_cast<int>(MessageType::InitialRequest):
        {
            //解析出消息，得到id，存储下来
            std::string userId=pt.get<std::string>("UserId");
            m_iId=atoi(userId.c_str());
            m_ptrChatServer->insertIntoIdMap(atoi(userId.c_str()),shared_from_this());
            MysqlQuery::Instance()->updateUserOnlineState(userId,true);
            //查询表获取好友列表，并发送
            GetFriendListReplyData getFriendListReplyData;
            MysqlQuery::Instance()->queryUserFrinedList(getFriendListReplyData.m_vecFriendList,userId);
            auto sendStr=getFriendListReplyData.generateJson();
            DoWrite(sendStr,getFriendListReplyData.generateJson().length());
            //TODO推送缓存的聊天消息和添加好友请求
            std::vector<MyAddFriendInfo> vecCachedAddFriend;
            MysqlQuery::Instance()->queryCachedAddFriendInfo(vecCachedAddFriend,userId);
            if(vecCachedAddFriend.size()>0)
            {
                MysqlQuery::Instance()->deleteCachedAddFriendInfo(userId);
            }
            for(auto& item:vecCachedAddFriend)
            {
                AddFriendRequestJsonData tmp;
                tmp.m_strFriendId=item.m_strFriendId;
                tmp.m_strMyId=item.m_strMyId;
                tmp.m_strName=MysqlQuery::Instance()->queryUserNameAcordId(tmp.m_strFriendId);
                tmp.m_strVerifyMsg=item.m_strVerifyMsg;
                DoWrite(tmp.generateJson(),tmp.generateJson().length());
            }
            std::vector<MyChatMessageInfo> vecCachedChatInfo;
            MysqlQuery::Instance()->queryCachedChatMsg(vecCachedChatInfo,userId);
            if(vecCachedChatInfo.size()>0)
            {
                MysqlQuery::Instance()->deleteCachedChatMsg(userId);
            }
            for(auto & item:vecCachedChatInfo)
            {
                SingleChatMessageJsonData tmp;
                tmp.m_strMessage=item.m_strChatMsg;
                tmp.m_strRecvUserId=item.m_strToId;
                tmp.m_strSendUserId=item.m_strFromId;
                tmp.m_strSendName=item.m_strSendName;
                DoWrite(tmp.generateJson(),tmp.generateJson().length());

                //把头像也发送过去
                auto imagePath=MysqlQuery::Instance()->queryImagePathAcordId(item.m_strFromId);
                std::string timeStamp=MysqlQuery::Instance()->queryImageTimeStampAcordId(item.m_strFromId);
                std::string suffix=imagePath.substr(imagePath.find_last_of('.') + 1);//获取文件后缀
                boost::uuids::uuid uid = boost::uuids::random_generator()();
                const std::string uid_str = boost::uuids::to_string(uid);
                if(imagePath!="")
                {
                    std::fstream in(imagePath,std::ios::in);
                    if(in.is_open())
                    {
                        std::string base64Str;
                        in>>base64Str;
                        in.close();
                        int iSumIndex=base64Str.length()/kSegmentLength;
                        if(base64Str.length()%kSegmentLength!=0)
                        {
                            iSumIndex++;
                        }
                        for(int i=0;i<iSumIndex;i++)
                        {
                            ProfileImageMsgJsonData profileImageMsgJsonData;
                            profileImageMsgJsonData.m_strId=item.m_strFromId;
                            profileImageMsgJsonData.m_strUUID=uid_str;
                            profileImageMsgJsonData.m_strSuffix=suffix;
                            profileImageMsgJsonData.m_strTimeStamp=timeStamp;
                            profileImageMsgJsonData.m_iCurIndex=i+1;
                            profileImageMsgJsonData.m_iSumIndex=iSumIndex;
                            profileImageMsgJsonData.m_strBase64Msg=base64Str.substr(i*kSegmentLength,kSegmentLength);
                            profileImageMsgJsonData.m_eImageType=ProfileImageType::AddFriendProfileImage;
                            auto sendStr=profileImageMsgJsonData.generateJson();
                            DoWrite(sendStr,sendStr.length());
                        }
                    }
                    else
                    {
                        _LOG(Logcxx::Level::ERROR,u8"打开文件失败");
                    }
                }
                else
                {
                    _LOG(Logcxx::Level::INFO,u8"数据库中没有该用户的头像");
                }
            }
        }
        break;
    //点对点聊天信息
    case static_cast<int>(MessageType::SingleChat):
        {
            SingleChatMessageJsonData singleChatData(message);
            //std::string recvId=pt.get<std::string>("RecvUserId");
            //获取一下要接受消息的人的在线状态
            bool onlineState=MysqlQuery::Instance()->queryUserIsOnline(singleChatData.m_strRecvUserId);
            //如果在线，就转发
            if(onlineState)
            {
                std::string sendMessage=message;
                m_ptrChatServer->transferMessage(atoi(singleChatData.m_strRecvUserId.c_str()),sendMessage);
            }
            else
            {
                //TODO插入数据库中
                MysqlQuery::Instance()->insertCachedChatMsg(singleChatData.m_strSendUserId,singleChatData.m_strRecvUserId,singleChatData.m_strMessage,singleChatData.m_strSendName,singleChatData.m_strTime);
            }
        }
        break;
    //获取好友列表的请求
    case static_cast<int>(MessageType::GetFriendList):
        {
            std::string userId=pt.get<std::string>("UserId");
            //TODO查询表获取好友列表
            GetFriendListReplyData getFriendListReplyData;
            MysqlQuery::Instance()->queryUserFrinedList(getFriendListReplyData.m_vecFriendList,userId);
            auto sendStr=getFriendListReplyData.generateJson();
            DoWrite(sendStr,getFriendListReplyData.generateJson().length());
        }
        break;
    //收到头像
    case static_cast<int>(MessageType::ProfileImageMsg):
        {
            ProfileImageMsgJsonData profileImageMsgData(message);
            int iNeedSegment=profileImageMsgData.m_iSumIndex;
            if(m_mapImageUUIDAndSegment.count(profileImageMsgData.m_strUUID)&&profileImageMsgData.m_iCurIndex-1 != m_mapImageUUIDAndSegment[profileImageMsgData.m_strUUID])
            {
                //TODO 回复一个uuid发送失败的消息
                m_mapImageUUIDAndBase64.erase(profileImageMsgData.m_strUUID);
                m_mapImageUUIDAndSegment.erase(profileImageMsgData.m_strUUID);
            }
            m_mapImageUUIDAndBase64[profileImageMsgData.m_strUUID]+=profileImageMsgData.m_strBase64Msg;
            if(profileImageMsgData.m_iCurIndex==profileImageMsgData.m_iSumIndex)
            {
                //如果收到的片数到达了最后一个了
                //将图片保存到本地，并将图片的路径保存到数据库中
                std::string curPath=getCurrentDir();
                //TODO从数据库获取上次的路径，删除上次的图片
                std::string oldPath=MysqlQuery::Instance()->queryImagePathAcordId(profileImageMsgData.m_strId);
                if(oldPath!="")
                {
                    remove(oldPath.c_str());
                }
                curPath+="/data/profileImage/"+profileImageMsgData.m_strImageName+"."+profileImageMsgData.m_strSuffix;
                std::fstream out(curPath,std::ios::out);
                if(out.is_open())
                {
                    out<<m_mapImageUUIDAndBase64[profileImageMsgData.m_strUUID];
                    out.close();
                }
                else{
                    _LOG(Logcxx::Level::ERROR,"保存头像时，打开文件失败");
                }
                MysqlQuery::Instance()->updateImagePathAcordId(profileImageMsgData.m_strId,curPath,profileImageMsgData.m_strTimeStamp);
                m_mapImageUUIDAndBase64.erase(profileImageMsgData.m_strUUID);
                m_mapImageUUIDAndSegment.erase(profileImageMsgData.m_strUUID);
                //TODO 回复一个发送成功的消息
            }
            m_mapImageUUIDAndSegment[profileImageMsgData.m_strUUID]=profileImageMsgData.m_iCurIndex;
        }
        break;
    //收到获取头像请求
    case static_cast<int>(MessageType::getFriendProfileImage):
        {
            GetProfileImageJsonData getFriendProfileImageJsonData(message);
            std::string imagePath=MysqlQuery::Instance()->queryImagePathAcordId(getFriendProfileImageJsonData.m_strId);
            std::string timeStamp=MysqlQuery::Instance()->queryImageTimeStampAcordId(getFriendProfileImageJsonData.m_strId);
            std::string suffix=imagePath.substr(imagePath.find_last_of('.') + 1);//获取文件后缀
            //printf("id:%s,timestamp:%s,suffix:%s",getFriendProfileImageJsonData.m_strId.c_str(),timeStamp.c_str(),suffix.c_str());
            boost::uuids::uuid uid = boost::uuids::random_generator()();
            const std::string uid_str = boost::uuids::to_string(uid);
            if(imagePath!="")
            {
                std::fstream in(imagePath,std::ios::in);
                if(in.is_open())
                {
                    std::string base64Str;
                    in>>base64Str;
                    in.close();
                    int iSumIndex=base64Str.length()/kSegmentLength;
                    if(base64Str.length()%kSegmentLength!=0)
                    {
                        iSumIndex++;
                    }
                    for(int i=0;i<iSumIndex;i++)
                    {
                        ProfileImageMsgJsonData profileImageMsgJsonData;
                        profileImageMsgJsonData.m_strId=getFriendProfileImageJsonData.m_strId;
                        profileImageMsgJsonData.m_strUUID=uid_str;
                        profileImageMsgJsonData.m_strSuffix=suffix;
                        profileImageMsgJsonData.m_strTimeStamp=timeStamp;
                        profileImageMsgJsonData.m_iCurIndex=i+1;
                        profileImageMsgJsonData.m_iSumIndex=iSumIndex;
                        profileImageMsgJsonData.m_strBase64Msg=base64Str.substr(i*kSegmentLength,kSegmentLength);
                        auto sendStr=profileImageMsgJsonData.generateJson();
                        DoWrite(sendStr,profileImageMsgJsonData.generateJson().length());
                    }
                }
                else{
                    _LOG(Logcxx::Level::ERROR,u8"打开文件失败");
                }
            }
            else{
                _LOG(Logcxx::Level::INFO,u8"数据库中没有该用户的头像");
            }
        }
        break;
    default:
        break;
    }
    return;
}

