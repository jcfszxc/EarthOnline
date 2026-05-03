/**
 * GetLidarData.cpp - 雷达数据获取和处理模块
 *
 * 本文件实现了基于多平台（Windows/Linux）的雷达数据获取和处理功能。
 * 主要功能包括：
 * 1. 网络通信 - 建立UDP Socket连接接收雷达数据和设备参数
 * 2. 数据处理 - 解析雷达数据包、帧数据管理和回调传递
 * 3. 参数配置 - 设置和管理雷达设备参数（IP地址、端口、旋转速度等）
 * 4. 设备控制 - 通过命令包实现对雷达设备的控制
 * 5. 数据过滤 - 根据多维条件（角度、距离、强度等）过滤雷达点云数据
 * 6. 多平台支持 - 通过条件编译实现Linux (epoll) 和Windows (IOCP) 的异步IO
 * 7. 资源管理 - 线程管理、内存管理和Socket资源处理
 *
 * 该模块用于雷达设备的数据采集和实时处理应用。
 */

// 包含头文件
#include "GetLidarData.h"

// 构造函数 - 初始化角度计算值
GetLidarData::GetLidarData()
{
    // 预计算所有可能角度的正弦和余弦值，优化后续计算性能
    for (long int FF = 0; FF < 360000; FF++)
    {
        cosAngleValue[FF] = cos(FF / 1000.0 * PI / 180);
        sinAngleValue[FF] = sin(FF / 1000.0 * PI / 180);
    }

    // 创建共享指针用于存储雷达数据
    LidarPerFrameDatePrt_Get = std::make_shared<std::vector<MuchLidarData>>();
}

// 析构函数
GetLidarData::~GetLidarData()
{
    // 无需特殊清理，智能指针会自动管理内存
}

// 设置网络端口和IP地址参数
void GetLidarData::setPortAndIP(uint16_t mDataPort, uint16_t mDevPort, std::string mDestIP, std::string mLidarIP, std::string mGroupIp)
{
    dataPort = mDataPort; // 数据端口
    devPort = mDevPort;   // 设备端口
    computerIP = mDestIP; // 计算机IP
    lidarIP = mLidarIP;   // 雷达IP
    groupIp = mGroupIp;   // 组播IP
}

// 修改雷达IP地址（当前版本不支持）
void GetLidarData::setChangeLidarIP(std::string mFixedLidarIP, std::string mLidarIP, std::string mDestIP, uint16_t mDataPort, uint16_t mDevPort)
{
    // 提示信息：当前版本不支持此功能
    std::string str = "This version of Lidar does not support 'setChangeLidarIP()'!!!";
    messFunction(str, 0);
    return;
}

// 发送第二IP（当前版本不支持）
bool GetLidarData::sendSecondIP(int switchIP, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'sendSecondIP()'!!!";
    return false;
}

// 发送第二提示灯信号（当前版本不支持）
bool GetLidarData::sendSecondCueLight(int switchIP, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'sendSecondCueLight()'!!!";
    return false;
}

// 设置数据回调函数
void GetLidarData::setCallbackFunction(FunDataPrt *callbackValue)
{
    callback = callbackValue;
}

// 启动雷达数据接收
void GetLidarData::LidarStart()
{
    isQuit = false;
    // 创建并分离线程用于运行雷达主处理函数
    std::thread t1(&GetLidarData::LidarRun, this);
    t1.detach();

#ifdef LINUX
    // Linux环境下创建Socket线程
    std::thread m_LinuxSockT(&GetLidarData::LinuxSockCreate, this);
    m_LinuxSockT.detach();
#else
    // Windows环境下创建Socket线程
    std::thread m_WindowSockT(&GetLidarData::WindowSockCreate, this);
    m_WindowSockT.detach();
#endif
}

// 启动离线数据处理
void GetLidarData::LidarOfflineDataStar()
{
    isQuit = false;
    // 创建并分离线程用于运行雷达主处理函数（离线模式）
    std::thread t1(&GetLidarData::LidarRun, this);
    t1.detach();
}

#ifdef LINUX
// 设置Socket为非阻塞模式
int setNonBlocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
        return -1;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

// Linux环境下创建Socket并接收数据
int GetLidarData::LinuxSockCreate()
{
    // 创建epoll实例
    struct epoll_event events[2];
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Failed to create epoll file descriptor\n";
        std::string str = "Failed to create epoll file descriptor\n";
        messFunction(str, 10000);
        return 1;
    }

    int server_sockfd[2];
    struct sockaddr_in server_addr1;

    // 创建两个UDP Socket
    for (int i = 0; i < 2; ++i)
    {
        server_sockfd[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (server_sockfd[i] == -1)
        {
            std::cerr << "Failed to create socket\n";
            std::string str = "Failed to create socket!\n";
            messFunction(str, 10000);
            return 1;
        }

        memset(&server_addr1, 0, sizeof(server_addr1));

        server_addr1.sin_family = AF_INET;
        server_addr1.sin_addr.s_addr = INADDR_ANY;

        // 分别绑定数据端口和设备端口
        if (i == 0)
        {
            server_addr1.sin_port = htons(dataPort);
            // 可选：设置Socket接收缓冲区大小
            // int value = 10 * 1024 * 1024;
            // int tmpCode = 0;
            // tmpCode = ::setsockopt(server_sockfd[i], SOL_SOCKET, SO_RCVBUF, (char*)&value, sizeof(value));
        }
        else
        {
            server_addr1.sin_port = htons(devPort);
        }

        // 设置组播
        ip_mreq multiCast;
        multiCast.imr_interface.s_addr = INADDR_ANY;
        inet_pton(AF_INET, groupIp.data(), &multiCast.imr_multiaddr.s_addr);

        // 加入组播组
        if (setsockopt(server_sockfd[i], IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multiCast, sizeof(multiCast)) < 0)
        {
            std::string str = "Adding multicast groupIP error!\n";
            messFunction(str, 10000);
        }
        else
        {
            std::cout << "Adding multicast groupIP...OK.\n";
        }

        // 绑定Socket
        if (bind(server_sockfd[i], (struct sockaddr *)&server_addr1, sizeof(server_addr1)) == -1)
        {
            std::cerr << "Failed to bind socket\n";
            std::string str = "Failed to bind socket\n";
            messFunction(str, 10002);
            return 1;
        }

        // 设置非阻塞模式
        setNonBlocking(server_sockfd[i]);

        // 注册到epoll
        struct epoll_event event;
        // event.events = EPOLLIN | EPOLLET;  // 边缘触发
        event.events = EPOLLIN; // 水平触发
        event.data.fd = server_sockfd[i];

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sockfd[i], &event) == -1)
        {
            std::cerr << "Failed to add socket to epoll\n";
            std::string str = "Failed to add socket to epoll\n";
            messFunction(str, 10002);
            return 1;
        }
    }

    // 主循环：等待并处理网络事件
    while (true)
    {
        // 检查是否退出
        if (isQuit)
        {
            std::string str = "Exit to obtain network data!!!\n";
            messFunction(str, 10009);
            break;
        }

        // 等待事件，超时时间2秒
        int num_events = epoll_wait(epoll_fd, events, 2, 2000);
        if (num_events == -1)
        {
            std::cerr << "Error in epoll_wait\n";
            return 1;
        }
        else if (num_events == 0)
        {
            std::cout << "Timeout. No events occurred within " << 2000 << " ms\n";
            std::string str = "Timeout!!! 2s\n";
            messFunction(str, 10004);
            continue;
        }

        // 处理所有活动的事件
        for (int i = 0; i < num_events; ++i)
        {
            if (events[i].events & EPOLLIN)
            {
                unsigned char buffer[2048];
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);

                // 接收数据
                int bytes_received = recvfrom(events[i].data.fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
                if (bytes_received == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        std::string str = "No more data to read \n";
                        messFunction(str, 10004);
                    }
                    else
                    {
                        std::cerr << "Error in recvfrom\n";
                        std::string str = "Error in recvfrom \n";
                        messFunction(str, 10003);
                        return 1;
                    }
                }
                else
                {
                    // 复制数据并传递给处理函数
                    u_char data[1212] = {0};
                    memcpy(data, buffer, bytes_received);
                    CollectionDataArrive(data, bytes_received);
                }
            }
        }
    }

    // 清理资源
    close(epoll_fd);
    for (int i = 0; i < 2; ++i)
    {
        close(server_sockfd[i]);
    }

    return 0;
}

#else
// Windows环境下创建Socket并接收数据
void GetLidarData::WindowSockCreate()
{
    // 创建完成端口
    HANDLE completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (completionPort == NULL)
    {
        std::cerr << "CreateIoCompletionPort failed" << std::endl;
        std::string str = "CreateIoCompletionPort failed!\n";
        messFunction(str, 10000);
        return;
    }

    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        std::string str = "WSAStartup failed!\n";
        messFunction(str, 10000);
        return;
    }

    // 创建数据Socket
    m_SockData = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_SockData == INVALID_SOCKET)
    {
        std::cerr << "The m_SockData socket create failed" << std::endl;
        std::string str = "The m_SockData socket create failed!\n";
        messFunction(str, 10000);
        return;
    }

    // 设置地址信息
    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(dataPort);
    inet_pton(AF_INET, computerIP.c_str(), &sockAddr.sin_addr);

    // 设置接收缓冲区大小
    int value = 10 * 1024 * 1024;
    int tmpCode = 0;
    tmpCode = ::setsockopt(m_SockData, SOL_SOCKET, SO_RCVBUF, (char *)&value, sizeof(value));

    // 设置组播
    ip_mreq multiCast;
    multiCast.imr_interface.S_un.S_addr = INADDR_ANY;
    inet_pton(AF_INET, groupIp.data(), &multiCast.imr_multiaddr.S_un.S_addr);

    // 加入组播组
    if (setsockopt(m_SockData, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multiCast, sizeof(multiCast)) < 0)
    {
        std::string str = "Adding multicast groupIP error!\n";
        messFunction(str, 10000);
    }
    else
    {
        std::cout << "Adding multicast groupIP...OK.\n";
    }

    // 绑定数据Socket
    if (bind(m_SockData, (SOCKADDR *)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Error binding server socket" << std::endl;
        std::string str = "Bind m_SockData failed!!!\n";
        messFunction(str, 10001);
        return;
    }

    // 创建设备Socket
    m_SockDev = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_SockDev == INVALID_SOCKET)
    {
        std::cerr << "The m_SockDev socket create failed" << std::endl;
        std::string str = "The m_SockDev socket create failed!\n";
        messFunction(str, 10000);
        return;
    }

    // 设置设备Socket地址
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(devPort);
    inet_pton(AF_INET, computerIP.c_str(), &sockAddr.sin_addr);

    // 设备Socket加入组播组
    multiCast.imr_interface.S_un.S_addr = INADDR_ANY;
    inet_pton(AF_INET, groupIp.data(), &multiCast.imr_multiaddr.S_un.S_addr);

    if (setsockopt(m_SockDev, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multiCast, sizeof(multiCast)) < 0)
    {
        std::string str = "Adding multicast groupIP error!\n";
        messFunction(str, 10000);
    }
    else
    {
        std::cout << "Adding multicast groupIP...OK.\n";
    }

    // 绑定设备Socket
    if (bind(m_SockDev, (SOCKADDR *)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Error binding server socket" << std::endl;
        std::string str = "Bind m_SockDev failed!!!\n";
        messFunction(str, 10011);
        return;
    }

    // 将Socket关联到完成端口
    CreateIoCompletionPort((HANDLE)m_SockData, completionPort, (ULONG_PTR)m_SockData, 0);
    CreateIoCompletionPort((HANDLE)m_SockDev, completionPort, (ULONG_PTR)m_SockDev, 0);

    // 初始化异步IO操作所需的结构
    SocketContext context1 = {m_SockData, {0}, WSAOVERLAPPED(), {1212, context1.buffer}};
    SocketContext context2 = {m_SockDev, {0}, WSAOVERLAPPED(), {1212, context2.buffer}};

    // 开始异步接收
    DWORD flags = 0;
    DWORD bytesTransferred = 0;

    // 启动数据Socket的异步接收
    int result = WSARecv(m_SockData, &context1.dataBuf, 1, &bytesTransferred, &flags, &context1.overlapped, NULL);
    if (result == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            std::cerr << "m_SockData WSARecvFrom failed" << std::endl;
            std::string str = "The m_SockData failed to obtain data!!!\n";
            messFunction(str, 10003);
            return;
        }
    }

    // 启动设备Socket的异步接收
    result = WSARecv(m_SockDev, &context2.dataBuf, 1, &bytesTransferred, &flags, &context2.overlapped, NULL);
    if (result == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            std::cerr << "m_SockDev WSARecvFrom failed" << std::endl;
            std::string str = "The m_SockDev failed to obtain data!!!\n";
            messFunction(str, 10013);
            return;
        }
    }

    // 主循环：处理完成端口事件
    while (true)
    {
        // 检查是否退出
        if (isQuit)
        {
            std::string str = "Exit to obtain network data!!!\n";
            messFunction(str, 10019);
            break;
        }

        DWORD dwBytes;
        ULONG_PTR ulKey;
        LPOVERLAPPED lpOverlapped;

        // 等待完成端口事件，超时时间为1秒
        BOOL bResult = GetQueuedCompletionStatus(completionPort, &dwBytes, &ulKey, &lpOverlapped, 1000);

        if (!bResult)
        {
            // 处理错误
            DWORD dwError = GetLastError();
            if (dwError == ERROR_NETNAME_DELETED)
            {
                // 客户端断开连接
            }
            else
            {
                std::cout << "timeOut Error!: " << dwError << std::endl;
                std::string str = " socket recvfrom timeout ! Failed to obtain data!\n";
                messFunction(str, 10004);
            }
        }
        else
        {
            // 处理数据Socket的接收完成事件
            if (ulKey == (ULONG_PTR)m_SockData)
            {
                // 复制接收到的数据并处理
                u_char data[1212] = {0};
                memcpy(data, context1.dataBuf.buf, dwBytes);
                CollectionDataArrive(data, dwBytes);

                // 启动下一次异步接收
                WSARecv(m_SockData, &context1.dataBuf, 1, &bytesTransferred, &flags, &context1.overlapped, NULL);
            }

            // 处理设备Socket的接收完成事件
            if (ulKey == (ULONG_PTR)m_SockDev)
            {
                // 复制接收到的数据并处理
                u_char data[1212] = {0};
                memcpy(data, context2.dataBuf.buf, dwBytes);
                CollectionDataArrive(data, dwBytes);

                // 启动下一次异步接收
                WSARecv(m_SockDev, &context2.dataBuf, 1, &bytesTransferred, &flags, &context2.overlapped, NULL);
            }
        }
    }

    // 清理资源
    closesocket(m_SockData);
    closesocket(m_SockDev);
    CloseHandle(completionPort);
    WSACleanup();
}
#endif

// 获取数据包状态信息
std::string GetLidarData::getDataPacketState()
{
    m_mutex.lock();
    std::string mDataInfoStringT = mDataInfoString;
    m_mutex.unlock();

    return mDataInfoStringT;
}

// 获取设备包状态信息
std::string GetLidarData::getDevPacketState()
{
    m_mutex.lock();
    std::string mDevInfoStringT = mDataInfoString;
    m_mutex.unlock();

    return mDevInfoStringT;
}

// 获取雷达参数状态
bool GetLidarData::getLidarParamState(LidarStateParam &mLidarStateParam, std::string &InfoString)
{
    // 检查设备包是否已更新
    if (true == islidarDevCome)
    {
        m_mutex.lock();
        // 保存设备包数据
        unsigned char pktdata[1206];
        memcpy(pktdata, dataDev, 1206);
        m_mutex.unlock();

        // 解析电机转速
        float mMotorSpeed = pktdata[8] * 256 + pktdata[9];
        mLidarStateParam.MotorSpeed = mMotorSpeed;

        // 解析雷达IP地址
        std::string ip_value =
            std::to_string(pktdata[10]) + "." +
            std::to_string(pktdata[11]) + "." +
            std::to_string(pktdata[12]) + "." +
            std::to_string(pktdata[13]);
        mLidarStateParam.LidarIP = ip_value;

        // 解析目标IP地址（计算机IP）
        ip_value =
            std::to_string(pktdata[14]) + "." +
            std::to_string(pktdata[15]) + "." +
            std::to_string(pktdata[16]) + "." +
            std::to_string(pktdata[17]);
        mLidarStateParam.ComputerIP = ip_value;

        // 解析NTP服务器IP
        ip_value =
            std::to_string(pktdata[28]) + "." +
            std::to_string(pktdata[29]) + "." +
            std::to_string(pktdata[30]) + "." +
            std::to_string(pktdata[31]);
        mLidarStateParam.NtpIP = ip_value;

        // 解析网关IP
        ip_value =
            std::to_string(pktdata[32]) + "." +
            std::to_string(pktdata[33]) + "." +
            std::to_string(pktdata[34]) + "." +
            std::to_string(pktdata[35]);
        mLidarStateParam.GatewayIP = ip_value;

        // 解析子网掩码
        ip_value =
            std::to_string(pktdata[36]) + "." +
            std::to_string(pktdata[37]) + "." +
            std::to_string(pktdata[38]) + "." +
            std::to_string(pktdata[39]);
        mLidarStateParam.SubnetMaskIP = ip_value;

        // 解析MAC地址
        ip_value =
            IntToHex(pktdata[18]) + "-" +
            IntToHex(pktdata[19]) + "-" +
            IntToHex(pktdata[20]) + "-" +
            IntToHex(pktdata[21]) + "-" +
            IntToHex(pktdata[22]) + "-" +
            IntToHex(pktdata[23]);
        mLidarStateParam.MacAddress = ip_value;

        // 解析数据端口和设备端口
        mLidarStateParam.DataPort = pktdata[24] * 256 + pktdata[25];
        mLidarStateParam.DevPort = pktdata[26] * 256 + pktdata[27];

        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 停止雷达数据接收
void GetLidarData::LidarStop()
{
    isQuit = true;
    // 等待1秒确保线程有时间退出
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
// 发送雷达数据到回调函数
void GetLidarData::sendLidarData()
{
    // 检查距离值是否合理（如果所有距离值为0，表示数据异常）
    if (m_DistanceIsNotZero < 20)
    {
        // 发出数据错误警告
        messFunction("Data error!!! All lidar distanceValue are 0!", 10032);
    }
    else
    {
        // 数据正常，更新状态信息
        m_mutex.lock();
        isSuccessfulFlag = true;
        mDataInfoString = "Obtaining data successfully!";
        m_mutex.unlock();
    }
    // 重置非零距离计数器
    m_DistanceIsNotZero = 0;

    // 如果设置了回调函数，则调用回调传递数据
    if (callback)
    {
        LidarPerFrameDatePrt_Send = LidarPerFrameDatePrt_Get;
        (*callback)(LidarPerFrameDatePrt_Send, isSuccessfulFlag, mDataInfoString);
    }

    // 更新帧数据并准备下一帧数据的缓冲区
    m_mutex.lock();
    LidarPerFrameDatePer = LidarPerFrameDatePrt_Get;
    LidarPerFrameDatePrt_Get.reset(new std::vector<MuchLidarData>);
    isFrameOK = true;
    m_mutex.unlock();
}

// 处理接收到的数据包
void GetLidarData::CollectionDataArrive(void *pData, uint16_t len)
{
    // 确保数据包长度足够
    if (len >= 1206)
    {
        // 创建缓冲区并复制数据
        unsigned char *dataV = new unsigned char[1212];
        memset(dataV, 0, 1212);
        memcpy(dataV, pData, len);

        // 将数据加入队列
        m_mutex.lock();
        allDataValue.push(dataV);
        m_mutex.unlock();

        // 检查数据包头，判断是否为设备信息包
        if ((dataV[0] == 0x00 || dataV[0] == 0xa5) && dataV[1] == 0xff && dataV[2] == 0x00 && dataV[3] == 0x5a)
        {
            // 是设备信息包，保存设备信息
            m_mutex.lock();
            memcpy(dataDev, pData, 1206);

            // 解析设备IP地址
            ip_sa = std::to_string(dataDev[10]) + "." +
                    std::to_string(dataDev[11]) + "." +
                    std::to_string(dataDev[12]) + "." +
                    std::to_string(dataDev[13]);

            // 如果有索引参数，输出当前索引的值
            if (setIndex > 0)
                std::cout << "Current index parameters:" << setIndex << "	The read-back value is: " << ": " << (setIndex < 0 ? 0 : int(dataV[setIndex])) << std::endl;
            islidarDevCome = true;
            m_mutex.unlock();
        }
    }
}

// 将负角度值转换为正值（0-360度范围内）
int GetLidarData::NegativeToPositive(float value)
{
    // 转换为毫度（千分之一度）
    int valueT = value * 1000;
    if (valueT >= 0)
    {
        // 如果为正值，确保在0-360000范围内
        return (valueT > 360000 ? valueT % 360000 : valueT);
    }
    else
    {
        // 如果为负值，转为相应的正值（加360000）
        return (valueT < -360000 ? (valueT % -360000) + 360000 : valueT + 360000);
    }
}

// 将无符号字符转换为二进制字符串
std::string GetLidarData::ucharToBinaryStr(const unsigned char value)
{
    std::string binaryStr;
    // 从最高位开始，逐位转换为字符'0'或'1'
    for (int i = 7; i >= 0; i--)
    {
        binaryStr += (value & (1 << i)) ? '1' : '0';
    }
    return binaryStr;
}

// 清空队列
void GetLidarData::clearQueue(std::queue<unsigned char *> &m_queue)
{
    std::queue<unsigned char *> empty;
    swap(empty, m_queue);
}

// 获取雷达当前帧数据
bool GetLidarData::getLidarPerFrameDate(std::shared_ptr<std::vector<MuchLidarData>> &preFrameData, std::string &Info)
{
    m_mutex.lock();
    isFrameOK = false;
    // 移动当前帧数据到输出参数
    preFrameData = std::move(LidarPerFrameDatePer);
    Info = mDataInfoString;
    m_mutex.unlock();
    return isSuccessfulFlag;
}

// 设置雷达旋转速度
bool GetLidarData::setLidarRotateSpeed(int SpeedValue, std::string &InfoString)
{
    // 保存速度值
    m_SpeedValue = SpeedValue;

    // 准备设备参数缓冲区
    if (setLidarParam())
    {
        // 设置旋转速度（高字节和低字节）
        Rest_UCWP_buff[8] = SpeedValue / 256;
        Rest_UCWP_buff[9] = SpeedValue % 256;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
    // 启动定时检查线程
    startSleepThread();
    return true;
}

// 延时一段时间后启用速度检查
void GetLidarData::sleepTime()
{
    is_speedFlag = false;
    // 等待10秒
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    is_speedFlag = true;
}

// 启动速度检查线程
void GetLidarData::startSleepThread()
{
    std::thread t1_Start(&GetLidarData::sleepTime, this);
    t1_Start.detach();
}

// 设置雷达IP地址
bool GetLidarData::setLidarIP(std::string LidarIPValue, std::string &InfoString)
{
    // 验证IP地址格式
    std::regex ipv4(
        "\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b");
    if (!regex_match(LidarIPValue, ipv4))
    {
        InfoString = "The IP format entered is incorrect, please check the input parameters";
        return false;
    }

    if (setLidarParam())
    {
        // 解析IP地址字符串
        std::string::size_type defailtIP_pos;
        std::vector<std::string> IP_result;
        IP_result.clear();
        LidarIPValue = LidarIPValue + ".";               // 添加额外的点以便于获取最后一段数据
        for (size_t i = 0; i < LidarIPValue.size(); i++) // 分割IP地址
        {
            defailtIP_pos = LidarIPValue.find(".", i);
            if (defailtIP_pos < LidarIPValue.size())
            {
                std::string s = LidarIPValue.substr(i, defailtIP_pos - i);
                IP_result.emplace_back(std::move(s));
                i = defailtIP_pos;
            }
        }

        // 检查IP段数量是否完整
        if (IP_result.size() < 4)
        {
            InfoString = "Please enter the full Lidar IP address!!!Failed to set the Lidar IP address!!!";
            return false;
        }
        else if (IP_result.size() == 4 && IP_result[3] == "")
        {
            InfoString = "Please enter the full Lidar IP address!!!Failed to set the Lidar IP address!!!";
            return false;
        }

        // 检查IP地址是否符合要求
        if (!checkDefaultIP(IP_result, InfoString))
        {
            InfoString = "Failed to set the Lidar IP address!!!";
            return false;
        }

        // 将IP地址各段设置到配置缓冲区
        Rest_UCWP_buff[10] = atoi(IP_result[0].c_str());
        Rest_UCWP_buff[11] = atoi(IP_result[1].c_str());
        Rest_UCWP_buff[12] = atoi(IP_result[2].c_str());
        Rest_UCWP_buff[13] = atoi(IP_result[3].c_str());
        InfoString = "Successfully set!";
        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 检查雷达IP地址是否符合规范
bool GetLidarData::checkDefaultIP(std::vector<std::string> m_DefaultIP, std::string &InfoString)
{
    int HeadDefaultIPValue = stoi(m_DefaultIP[0]);
    int endDefaultIPValue = stoi(m_DefaultIP[3]);
    // 检查首段IP是否为保留地址或多播地址
    if (HeadDefaultIPValue == 0 || HeadDefaultIPValue == 127 ||
        (HeadDefaultIPValue >= 224 && HeadDefaultIPValue <= 255))
    {
        std::string str =
            "The Lidar IP cannot be set to " + std::to_string(HeadDefaultIPValue) + std::string(".x.x.x!!!");
        InfoString = str;
        messFunction(str, 0);
        return false;
    }
    // 检查末段IP是否为广播地址
    else if (endDefaultIPValue == 255)
    {
        std::string str = "The Lidar IP cannot be set to broadcast(x.x.x.255)!!!";
        InfoString = str;
        messFunction(str, 0);
        return false;
    }
    return true;
}

// 检查目标IP地址是否符合规范
bool GetLidarData::checkDestIP(std::vector<std::string> m_DestIP, std::string &InfoString)
{
    int HeadDefaultIPValue = stoi(m_DestIP[0]);
    // 检查首段IP是否为保留地址或多播地址
    if (HeadDefaultIPValue == 0 || HeadDefaultIPValue == 127 ||
        (HeadDefaultIPValue >= 240 && HeadDefaultIPValue <= 255))
    {
        std::string str =
            "The Dest IP cannot be set to " + std::to_string(HeadDefaultIPValue) + std::string(".x.x.x!!!");
        InfoString = str;
        messFunction(str, 0);
        return false;
    }
    return true;
}

// 设置计算机IP地址（雷达的目标IP）
bool GetLidarData::setComputerIP(std::string ComputerIPValue, std::string &InfoString)
{
    // 验证IP地址格式
    std::regex ipv4(
        "\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b");
    if (!regex_match(ComputerIPValue, ipv4))
    {
        InfoString = "The IP format entered is incorrect, please check the input parameters";
        return false;
    }

    if (setLidarParam())
    {
        // 解析IP地址字符串
        std::vector<std::string> IP_result;
        IP_result.clear();
        std::string::size_type DestIP_pos;
        ComputerIPValue = ComputerIPValue + ".";
        for (size_t i = 0; i < ComputerIPValue.size(); i++) // 分割IP地址
        {
            DestIP_pos = ComputerIPValue.find(".", i);
            if (DestIP_pos < ComputerIPValue.size())
            {
                std::string s = ComputerIPValue.substr(i, DestIP_pos - i);
                IP_result.emplace_back(std::move(s));
                i = DestIP_pos;
            }
        }

        // 检查IP段数量是否完整
        if (IP_result.size() < 4)
        {
            InfoString = "Please enter the full Dest IP address!!! Failed to set the Dest(Computer) IP address!!!";
            return false;
        }
        else if (IP_result.size() == 4 && IP_result[3] == "")
        {
            InfoString = "Please enter the full Dest IP address!!!Failed to set the Dest(Computer) IP address!!!";
            return false;
        }
        // 检查IP地址是否符合要求
        if (!checkDestIP(IP_result, InfoString))
        {
            InfoString = "Failed to set the computer IP address!!!";
            return false;
        }
        // 将IP地址各段设置到配置缓冲区
        Rest_UCWP_buff[14] = atoi(IP_result[0].c_str());
        Rest_UCWP_buff[15] = atoi(IP_result[1].c_str());
        Rest_UCWP_buff[16] = atoi(IP_result[2].c_str());
        Rest_UCWP_buff[17] = atoi(IP_result[3].c_str());

        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 设置NTP服务器IP地址（当前版本不支持）
bool GetLidarData::setNTP_IP(std::string IPString, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setNTP_IP()'!!!";
    return false;
}

// 设置网关IP地址（当前版本不支持）
bool GetLidarData::setGatewayIP(std::string IPString, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setGatewayIP()'!!!";
    return false;
}

// 设置子网掩码IP（当前版本不支持）
bool GetLidarData::setSubnetMaskIP(std::string IPString, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setSubnetMaskIP()'!!!";
    return false;
}

// 设置数据包端口
bool GetLidarData::setDataPort(int DataPort, std::string &InfoString)
{
    if (setLidarParam())
    {
        // 获取当前设备端口
        int devPort = Rest_UCWP_buff[26] * 256 + Rest_UCWP_buff[27];
        // 检查端口号是否合法（不能与设备端口相同，且须在有效范围内）
        if (DataPort < 1025 || DataPort > 65535 || DataPort == devPort)
        {
            InfoString = "DataPort range 1025-65535 and DataPort and devport cannot be equal, please check the input parameters";
            return false;
        }
        else
        {
            // 设置数据包端口（高字节和低字节）
            Rest_UCWP_buff[24] = DataPort / 256;
            Rest_UCWP_buff[25] = DataPort % 256;
            return true;
        }
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 设置设备包端口
bool GetLidarData::setDevPort(int DevPort, std::string &InfoString)
{
    if (setLidarParam())
    {
        // 获取当前数据端口
        int dataPort = Rest_UCWP_buff[24] * 256 + Rest_UCWP_buff[25];
        // 检查端口号是否合法（不能与数据端口相同，且须在有效范围内）
        if (DevPort < 1025 || DevPort > 65535 || DevPort == dataPort)
        {
            InfoString = "DataPort range 1025-65535 and DataPort and devport cannot be equal, please check the input parameters";
            return false;
        }
        else
        {
            // 设置设备包端口（高字节和低字节）
            Rest_UCWP_buff[26] = DevPort / 256;
            Rest_UCWP_buff[27] = DevPort % 256;
            return true;
        }
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 设置雷达旋转状态（当前版本不支持）
bool GetLidarData::setLidarRotateState(int RotateState, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setLidarRotateState()'!!!";
    return false;
}

// 设置雷达信号源选择（当前版本不支持）
bool GetLidarData::setLidarSoureSelection(int StateValue, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setLidarSoureSelection()'!!!";
    return false;
}

// 设置雷达工作状态（当前版本不支持）
bool GetLidarData::setLidarWorkState(int LidarState, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setLidarWorkState()'!!!";
    return false;
}

// 设置帧率模式（当前版本不支持）
bool GetLidarData::setFrameRateMode(int StateValue, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setFrameRateMode()'!!!";
    return false;
}

// 设置相位锁定开关（当前版本不支持）
bool GetLidarData::setPhaseLockedSwitch(int StateValue, std::string &InfoString)
{
    InfoString = "This version of Lidar does not support 'setPhaseLockedSwitch()'!!!";
    return false;
}

// 设置雷达索引参数值（通用参数设置）
bool GetLidarData::setLidarIndexParamValue(int ControlValue, int ChangeValue, std::string &InfoString)
{
    if (setLidarParam())
    {
        // 直接设置指定索引的参数值
        Rest_UCWP_buff[ControlValue] = ChangeValue;
        std::cout << "setLidarIndexParamValue() : ControlValue = " << ControlValue << " : ChangeValue = " << ChangeValue << std::endl;
        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 准备雷达参数配置缓冲区
bool GetLidarData::setLidarParam()
{
    // 如果已在发送UDP数据，则不再进行配置
    if (isSendUDP == false)
    {
        return true;
    }

    // 检查是否收到设备信息包
    if (true == islidarDevCome && true == isSendUDP)
    {
        islidarDevCome = false;
        isSendUDP = false;
        m_mutex.lock();
        // 将设备信息包复制到配置缓冲区，作为基础配置
        memcpy(Rest_UCWP_buff, dataDev, 1206);
        m_mutex.unlock();

        // 清除特定位置的值
        for (int i = 52; i < 60; i++)
        {
            Rest_UCWP_buff[i] = 0x00;
        }

        for (int i = 160; i < 168; i++)
        {
            Rest_UCWP_buff[i] = 0x00;
        }

        // 设置用户配置包的标识头
        Rest_UCWP_buff[0] = 0xAA; // 将UCWP与ACWP合并，UCWP标识头为前8字节
        Rest_UCWP_buff[1] = 0x00;
        Rest_UCWP_buff[2] = 0xFF;
        Rest_UCWP_buff[3] = 0x11;
        Rest_UCWP_buff[4] = 0x22;
        Rest_UCWP_buff[5] = 0x22;
        Rest_UCWP_buff[6] = 0xAA;
        Rest_UCWP_buff[7] = 0xAA;
        return true;
    }
    else
    {
        std::string str = "Equipment package is not update!!!";
        messFunction(str, 0);
        return false;
    }
}

// 向雷达发送数据包
bool GetLidarData::sendPacketToLidar(unsigned char *packet, const char *ip_data, u_short portNum)
{
#ifdef __linux__
    // Linux平台代码
    struct sockaddr_in addrSrv{};
    // 创建UDP套接字
    int socketid = socket(2, 2, 0);
#define SOCKET_ERROR -1
#else
    // Windows平台代码
    // 初始化Winsock
    WORD request;
    WSADATA ws;
    request = MAKEWORD(1, 1);
    int err = WSAStartup(request, &ws);
    if (err != 0)
    {
        return false;
    }
    if (LOBYTE(ws.wVersion) != 1 || HIBYTE(ws.wVersion) != 1)
    {
        WSACleanup();
        return false;
    }
    SOCKADDR_IN addrSrv;
    // 创建UDP套接字
    SOCKET socketid = socket(2, 2, 0);
#endif

    // 设置目标地址和端口
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(portNum);
    inet_pton(AF_INET, ip_data, &addrSrv.sin_addr);

    // 发送数据包
    int sd = sendto(socketid, (const char *)packet, 1206, 0, (struct sockaddr *)&addrSrv, sizeof(addrSrv));

    if (sd != SOCKET_ERROR)
    {
        printf("send successfully,send:%dchars\n", sd);
        // 延时等待
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifdef __linux__
        // Linux关闭套接字
        (void)::close(socketid);
#else
        // Windows关闭套接字
        closesocket(socketid);
#endif
    }
    else
    {
        printf("Failure to send\n");
        return false;
    }

    isSendUDP = true;
    return true;
}

// 发送UDP配置包到雷达
bool GetLidarData::sendPackUDP()
{
    return sendPacketToLidar(Rest_UCWP_buff, ip_sa.c_str(), 2368);
}

// 消息处理函数
void GetLidarData::messFunction(std::string strValue, int gValue)
{
    // 输出消息和代码到控制台
    std::cout << "Code = " << gValue << " : " << strValue.c_str() << std::endl;

    m_mutex.lock();
    // 根据错误码范围设置相应的状态信息
    if ((10000 <= gValue && gValue <= 10009) || (10030 <= gValue && gValue <= 10039))
    {
        isSuccessfulFlag = false;
        mDataInfoString = strValue;
    }

    if ((10010 <= gValue && gValue <= 10019))
    {
        mDevInfoString = strValue;
    }
    m_mutex.unlock();
}

// 设置雷达过滤显示参数
bool GetLidarData::setLidarFilterdisplay(LidaFilterParamDisplay mLidaFilterParamDisplay)
{
    mLidaFilterParamDisplayValue = mLidaFilterParamDisplay;
    return true;
}

// 判断点是否通过过滤规则
bool GetLidarData::isPointFilter(const MuchLidarData mPoint)
{
    // 通道过滤
    if (mPoint.ID < static_cast<int>(mLidaFilterParamDisplayValue.mChannelVector.size()))
        if (0 == mLidaFilterParamDisplayValue.mChannelVector[mPoint.ID])
            return false;

    // 水平角度过滤
    if (mLidaFilterParamDisplayValue.mMin_HanleValue <= mLidaFilterParamDisplayValue.mMax_HanleValue)
    {
        // 正常区间判断
        if (!(mLidaFilterParamDisplayValue.mMin_HanleValue <= mPoint.H_angle && mPoint.H_angle <= mLidaFilterParamDisplayValue.mMax_HanleValue))
            return false;
    }
    else
    {
        // 跨零区间判断
        if (mLidaFilterParamDisplayValue.mMax_HanleValue < mPoint.H_angle && mPoint.H_angle < mLidaFilterParamDisplayValue.mMin_HanleValue)
            return false;
    }

    // 垂直角度过滤
    if (!(mLidaFilterParamDisplayValue.mMin_VanleValue <= mPoint.V_angle && mPoint.V_angle <= mLidaFilterParamDisplayValue.mMax_VanleValue))
        return false;

    // 距离过滤
    if (!(mLidaFilterParamDisplayValue.mMin_Distance <= mPoint.Distance && mPoint.Distance <= mLidaFilterParamDisplayValue.mMax_Distance))
        return false;

    // 强度过滤
    if (!(mLidaFilterParamDisplayValue.mMin_Intensity <= mPoint.Intensity && mPoint.Intensity <= mLidaFilterParamDisplayValue.mMax_Intensity))
        return false;

    // 所有过滤条件都通过
    return true;
}