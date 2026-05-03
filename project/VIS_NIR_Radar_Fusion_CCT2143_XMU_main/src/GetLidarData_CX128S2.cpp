/**
 * GetLidarData_CX128S2.cpp - CX128S2激光雷达数据处理模块
 *
 * 本文件实现了CX128S2型激光雷达的数据处理和坐标转换功能。
 * 主要功能包括：
 * 1. 数据解析 - 解析单回波和双回波两种模式的激光雷达数据包
 * 2. 坐标转换 - 将极坐标系的点数据（角度+距离）转换为笛卡尔坐标系（XYZ）
 * 3. 时间同步 - 支持PTP（精确时间协议）和GPS两种时间同步方式
 * 4. 角度校准 - 处理垂直和水平角度偏移，提高点云精度
 * 5. 设备监控 - 获取并监控设备状态（温度、电压、运行时间等）
 * 6. 参数配置 - 设置设备工作模式（旋转状态、时钟源、工作状态等）
 * 7. 多线程处理 - 通过互斥锁保证数据处理的线程安全性
 * 8. 点云管理 - 点云数据的过滤、组织和帧同步
 *
 * 该模块作为CX128S2激光雷达驱动的核心组件，用于自动驾驶、机器人、测量等应用场景。
 */

#include "GetLidarData_CX128S2.h"

// 定义32个垂直通道的角度值（单位：度）
double G_Angle_CX128S2[32] = {
    -12, -11, -10, -9, -8, -7, -6, -5,
    -4.125, -4, -3.125, -3, -2.125, -2, -1.125, -1,
    -0.125, 0, 0.875, 1, 1.875, 2, 3, 4,
    5, 6, 7, 8, 9, 10, 11, 12};

GetLidarData_CX128S2::GetLidarData_CX128S2()
{
    // 构造函数：初始化角度的正弦和余弦值数组
    for (int i = 0; i < 128; i++)
    {
        // 计算垂直角的正弦值（通过通道索引整除4获取对应角度）
        sinTheta_1[i] = sinAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4])];
        // 计算水平微调角的正弦值（每4个通道对应一个-0.17度的角度偏移）
        sinTheta_2[i] = sinAngleValue[NegativeToPositive((i % 4) * (-0.17))];

        // 计算垂直角的余弦值
        cosTheta_1[i] = cosAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4])];
        // 计算水平微调角的余弦值
        cosTheta_2[i] = cosAngleValue[NegativeToPositive((i % 4) * (-0.17))];
    }

    count = 0; // 初始化计数器
}

GetLidarData_CX128S2::~GetLidarData_CX128S2()
{
    // 析构函数
}

void GetLidarData_CX128S2::LidarRun()
{
    // 激光雷达主运行函数，不断循环处理数据
    while (true)
    {
        if (isQuit)
        {
            // 如果收到退出信号，清空数据队列并退出循环
            clearQueue(allDataValue);
            break;
        }
        if (!allDataValue.empty())
        {
            // 如果数据队列非空，从队列中取出一帧数据
            unsigned char data[1212] = {0};
            m_mutex.lock();                           // 加锁，保证线程安全
            memcpy(data, allDataValue.front(), 1212); // 复制数据
            delete allDataValue.front();              // 释放内存
            allDataValue.pop();                       // 移除已处理的数据
            m_mutex.unlock();                         // 解锁

            // 检查是否为角度校准数据帧（通过魔数0xa5、0xff、0x00、0x5a识别）
            if (data[0] == 0xa5 && data[1] == 0xff && data[2] == 0x00 && data[3] == 0x5a)
            {
                // 提取棱镜角度偏移值
                std::vector<int> anglePrism;
                for (int i = 0; i < 5; i++)
                {
                    short int angleP = data[240 + i * 2] * 256 + data[241 + i * 2];
                    anglePrism.emplace_back(std::move(angleP));
                }
                // 更新角度偏移
                getOffsetAngle(anglePrism);

                continue; // 继续下一次循环
            }

            // 处理时间戳数据
            timestamp_nsce = (static_cast<uint64_t>(data[1206] << 24) +
                              static_cast<uint64_t>(data[1207] << 16) +
                              static_cast<uint64_t>(data[1208] << 8) + data[1209]);

            if (0xff == data[1200])
            { // PTP精确时间协议时间同步
                uint64_t _t = (static_cast<uint64_t>(data[1201] * 4294967296) +
                               static_cast<uint64_t>(data[1202] << 24) +
                               static_cast<uint64_t>(data[1203] << 16) +
                               static_cast<uint64_t>(data[1204] << 8) + data[1205]);

                // 计算完整时间戳（秒+纳秒）
                allTimestamp = _t * 1000000000 + timestamp_nsce;
            }
            else
            { // GPS时间同步
                // 解析UTC时间
                m_UTC_Time.year = data[1200] + 2000;
                m_UTC_Time.month = data[1201];
                m_UTC_Time.day = data[1202];
                m_UTC_Time.hour = data[1203];
                m_UTC_Time.minute = data[1204];
                m_UTC_Time.second = data[1205];

                // 转换UTC时间为时间戳
                struct tm t;
                t.tm_sec = m_UTC_Time.second;
                t.tm_min = m_UTC_Time.minute;
                t.tm_hour = m_UTC_Time.hour;
                t.tm_mday = m_UTC_Time.day;
                t.tm_mon = m_UTC_Time.month - 1;
                t.tm_year = m_UTC_Time.year - 1900;
                t.tm_isdst = 0;
                time_t _t = static_cast<uint64_t>(timegm(&t)); // 使用timegm获取UTC时间戳
                if (-1 == _t)
                {
                    perror("parse error");
                }
                // 计算完整时间戳（秒+纳秒）
                allTimestamp = _t * 1000000000 + timestamp_nsce;
            }

            // 获取激光雷达回波模式
            int lidar_EchoModel = data[1211];

            // 根据回波模式处理数据
            if (2 == lidar_EchoModel)
            {
                // 双回波模式处理
                handleDoubleEcho(data);
            }
            else
            {
                // 单回波模式处理
                handleSingleEcho(data);
            }

            lastAllTimestamp = allTimestamp; // 保存上一帧的时间戳
        }
        else
        {
            // 队列为空时，短暂休眠避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void GetLidarData_CX128S2::getOffsetAngle(std::vector<int> OffsetAngleValue)
{
    // 处理角度偏移值，用于校准激光雷达测量
    double prismAngle[4];
    float PrismOffsetVAngle = 0.0f;

    // 处理垂直角度偏移
    if (abs(OffsetAngleValue[0]) != 0)
    {
        PrismOffsetVAngle = OffsetAngleValue[0] * 0.01; // 转换为度

        for (int i = 0; i < 128; i++)
        {
            // 左侧通道应用偏移
            if (i / 4 % 2 == 0)
            {
                sinTheta_1[i] = sinAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4] + PrismOffsetVAngle)];
                cosTheta_1[i] = cosAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4] + PrismOffsetVAngle)];
            }
            else
            {
                // 右侧通道不应用偏移
                sinTheta_1[i] = sinAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4])];
                cosTheta_1[i] = cosAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4])];
            }
        }
    }
    else
    {
        // 无垂直偏移时使用默认角度
        for (int i = 0; i < 128; i++)
        {
            sinTheta_1[i] = sinAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4])];
            cosTheta_1[i] = cosAngleValue[NegativeToPositive(G_Angle_CX128S2[i / 4])];
        }
    }

    // 处理水平角度偏移
    if (abs(OffsetAngleValue[1]) == 0 && abs(OffsetAngleValue[2]) == 0 && abs(OffsetAngleValue[3]) == 0 &&
        abs(OffsetAngleValue[4]) == 0)
    {
        // 无水平偏移时使用默认值
        for (int i = 0; i < 4; i++)
        {
            prismAngle[i] = i * -0.17;
        }

        for (int i = 0; i < 128; i++)
        {
            sinTheta_2[i] = sinAngleValue[NegativeToPositive(prismAngle[i % 4])];
            cosTheta_2[i] = cosAngleValue[NegativeToPositive(prismAngle[i % 4])];
        }
    }
    else
    {
        // 应用水平偏移
        for (int i = 0; i < 4; i++)
        {
            prismAngle[i] = OffsetAngleValue[i + 1] * 0.01;
        }
        for (int i = 0; i < 128; i++)
        {
            sinTheta_2[i] = sinAngleValue[NegativeToPositive(prismAngle[i % 4])];
            cosTheta_2[i] = cosAngleValue[NegativeToPositive(prismAngle[i % 4])];
        }
    }
    return;
}

void GetLidarData_CX128S2::handleSingleEcho(unsigned char *data)
{
    // 处理单回波模式的数据
    // 计算点之间的时间间隔
    PointIntervalT_ns = (allTimestamp - lastAllTimestamp) * 1.0 / 171;

    // 逐个处理数据包中的点数据
    for (int i = 0; i < 1197; i = i + 7)
    {
        // 检查是否为帧同步标记（魔数）
        if (data[i] == 0xff && data[i + 1] == 0xaa && data[i + 2] == 0xbb && data[i + 3] == 0xcc &&
            data[i + 4] == 0xdd)
        {
            count++;
        }
        if (count == 1)
        {
            // 帧结束，发送当前帧的激光雷达数据
            sendLidarData();

            // 重置计数器和消息计数
            count = 0;
            m_messageCount = 0;
            continue;
        }
        // 检查缓冲区是否过大（可能是帧同步失败）
        if (LidarPerFrameDatePrt_Get->size() > 1000000)
        {
            if (1 == m_messageCount)
            {
                continue;
            }

            // 报告帧同步错误
            std::string str = "Frame synchronization failure!!!";
            messFunction(str, 10031);
            m_messageCount = 1;
            continue;
        }
        if (count == 0)
        {
            // 检查点ID是否有效
            if (data[i] < 128)
            {
                // 解析距离数据（单位：米）
                float m_Distance = (data[i + 3] * 65536 + data[i + 4] * 256 + data[i + 5]) * 0.0000390625;
                // 修正版本：安全地更新非零距离计数
                if (m_DistanceIsNotZero > 30)
                {
                    // 保持当前值
                }
                else
                {
                    // 非零距离判断（使用epsilon比较）
                    if (abs(m_Distance) > 0.00000001)
                    {
                        m_DistanceIsNotZero++;
                    }
                }

                // 解析其他点数据
                int m_ID = data[i];
                float m_H_angle = (data[i + 1] * 256 + data[i + 2]) * 0.01;
                int m_Intensity = data[i + 6];

                // 创建点数据结构
                MuchLidarData m_DataT;
                m_DataT.ID = m_ID;
                m_DataT.H_angle = m_H_angle;
                m_DataT.Distance = m_Distance;
                m_DataT.Intensity = m_Intensity;

                // 检查点是否通过过滤
                if (!isPointFilter(m_DataT))
                {
                    continue;
                }

                // 计算点的XYZ坐标
                m_PointXYZ m_point = XYZ_calculate(m_ID, m_H_angle, m_Distance, -1);
                m_DataT.X = m_point.x1;
                m_DataT.Y = m_point.y1;
                m_DataT.Z = m_point.z1;
                m_DataT.V_angle = m_point.V_angle;
                // 计算点的精确时间戳
                m_DataT.Mtimestamp_nsce = allTimestamp - PointIntervalT_ns * (170 - i / 7);
                // 添加到当前帧点云数据
                LidarPerFrameDatePrt_Get->emplace_back(std::move(m_DataT));
            }
        }
    }
}

void GetLidarData_CX128S2::handleDoubleEcho(unsigned char *data)
{
    // 处理双回波模式的数据
    // 计算点之间的时间间隔
    PointIntervalT_ns = (allTimestamp - lastAllTimestamp) * 1.0 / 109;

    // 逐个处理数据包中的点数据
    for (int i = 0; i < 1199; i = i + 11)
    {
        // 检查是否为帧同步标记（魔数）
        if (data[i] == 0xff && data[i + 1] == 0xaa && data[i + 2] == 0xbb && data[i + 3] == 0xcc &&
            data[i + 4] == 0xdd)
        {
            count++;
        }
        if (count == 1)
        {
            // 帧结束，发送当前帧的激光雷达数据
            sendLidarData();

            // 重置计数器和消息计数
            count = 0;
            m_messageCount = 0;
            continue;
        }
        // 检查缓冲区是否过大（可能是帧同步失败）
        if (LidarPerFrameDatePrt_Get->size() > 1000000)
        {
            if (1 == m_messageCount)
            {
                continue;
            }

            // 报告帧同步错误
            std::string str = "Frame synchronization failure!!!";
            messFunction(str, 10031);
            m_messageCount = 1;
            continue;
        }

        if (count == 0)
        {
            // 检查点ID是否有效
            if (data[i] < 128)
            {
                // 解析第一回波的距离数据
                float m_Distance = (data[i + 3] * 65536 + data[i + 4] * 256 + data[i + 5]) * 0.0000390625;
                // 修正版本：安全地更新非零距离计数
                if (m_DistanceIsNotZero > 30)
                {
                    // 保持当前值
                }
                else if (abs(m_Distance - 0) > 0.00000001)
                {
                    // 非零距离判断
                    m_DistanceIsNotZero++;
                }

                // 解析点的基本数据
                int m_ID = data[i];
                float m_H_angle = (data[i + 1] * 256 + data[i + 2]) * 0.01;
                int m_Intensity = data[i + 6];

                // 解析第二回波的数据
                float m_Distance_2 = (data[i + 7] * 65536 + data[i + 8] * 256 + data[i + 9]) * 0.0000390625;
                int m_Intensity_2 = (data[i + 10]);

                // 创建第一回波点数据
                MuchLidarData m_DataT;
                m_DataT.ID = m_ID;
                m_DataT.H_angle = m_H_angle;
                m_DataT.Distance = m_Distance;
                m_DataT.Intensity = m_Intensity;

                // 检查点是否通过过滤
                if (!isPointFilter(m_DataT))
                {
                    continue;
                }

                // 计算XYZ坐标（包括两个回波）
                m_PointXYZ m_point = XYZ_calculate(m_ID, m_H_angle, m_Distance, m_Distance_2);

                // 填充第一回波的完整数据
                m_DataT.X = m_point.x1;
                m_DataT.Y = m_point.y1;
                m_DataT.Z = m_point.z1;
                m_DataT.V_angle = m_point.V_angle;
                m_DataT.Mtimestamp_nsce = allTimestamp - PointIntervalT_ns * (108 - i / 11);
                // 添加到当前帧点云数据
                LidarPerFrameDatePrt_Get->emplace_back(std::move(m_DataT));

                // 创建并填充第二回波点数据
                MuchLidarData m_DataT_2;
                m_DataT_2.X = m_point.x2;
                m_DataT_2.Y = m_point.y2;
                m_DataT_2.Z = m_point.z2;
                m_DataT_2.ID = m_ID;
                m_DataT_2.H_angle = m_H_angle;
                m_DataT_2.V_angle = m_point.V_angle;
                m_DataT_2.Distance = m_Distance_2;
                m_DataT_2.Intensity = m_Intensity_2;
                m_DataT_2.Mtimestamp_nsce = allTimestamp - PointIntervalT_ns * (108 - i / 11);
                // 添加到当前帧点云数据
                LidarPerFrameDatePrt_Get->emplace_back(std::move(m_DataT_2));
            }
        }
    }
}

m_PointXYZ GetLidarData_CX128S2::XYZ_calculate(int ID, float H_angle, float Distance, float Distance2) // 坐标转换公式（Distance2 <= 0时不计算第二回波数据）
{
    m_PointXYZ point;

    // 计算投影因子R
    double _R_ = cosTheta_2[ID] * cosTheta_1[ID] * cosAngleValue[NegativeToPositive(H_angle / 2.0)] - sinTheta_2[ID] * sinTheta_1[ID];

    float sin_Theat;
    float cos_Theta;

    // 计算合成垂直角的正弦和余弦值
    sin_Theat = sinTheta_1[ID] + 2 * _R_ * sinTheta_2[ID];
    cos_Theta = sqrt(1 - sin_Theat * sin_Theat);

    // 计算垂直角度（度）
    point.V_angle = (asin(sin_Theat) * 180 / PI);

    // 计算第一回波点的XYZ坐标
    point.x1 = float(Distance * cos_Theta * cosAngleValue[NegativeToPositive(H_angle)]);
    point.y1 = float(Distance * cos_Theta * sinAngleValue[NegativeToPositive(H_angle)]);
    point.z1 = float(Distance * sin_Theat);

    // 如果存在第二回波，计算其XYZ坐标
    if (Distance2 > 0)
    {
        point.x2 = float(Distance2 * cos_Theta * cosAngleValue[NegativeToPositive(H_angle)]);
        point.y2 = float(Distance2 * cos_Theta * sinAngleValue[NegativeToPositive(H_angle)]);
        point.z2 = float(Distance2 * sin_Theat);
    }

    return point;
}

bool GetLidarData_CX128S2::getLidarParamState(LidarStateParam &mLidarStateParam, std::string &InfoString)
{
    // 获取激光雷达参数状态
    if (true == islidarDevCome)
    {
        // 调用基类方法获取基本参数
        GetLidarData::getLidarParamState(mLidarStateParam, InfoString);
        m_mutex.lock();
        // 保存设备数据包
        unsigned char pktdata[1206];
        memcpy(pktdata, dataDev, 1206);
        m_mutex.unlock();

        // 计算接收器温度（摄氏度）
        float fRXTemperature = (((pktdata[80] << 8) + pktdata[81]) / 4096.0) * 250 - 50;
        // 计算高压值（伏特）
        float fHV1 = 281 - 0.0692142 * (pktdata[84] * 256 + pktdata[85]);

        // 获取电机速度
        float mMotorSpeed = pktdata[8] * 256 + pktdata[9];

        // 计算运行时间（秒）
        double mRunningTime = pktdata[128] * pow(2, 24) + pktdata[129] * pow(2, 16) + pktdata[130] * pow(2, 8) + pktdata[131] * pow(2, 0);

        // 获取相位锁定角度
        float PhaseAngleLocke = (pktdata[47] * 256 + pktdata[48]) * 0.01;

        // 填充激光雷达状态参数
        mLidarStateParam.ReceiverTemperature = fRXTemperature;
        mLidarStateParam.ReceiverHighVoltage = fHV1;
        mLidarStateParam.MotorSpeed = mMotorSpeed;

        mLidarStateParam.GPS_State = pktdata[92];
        mLidarStateParam.PPS_State = pktdata[93];
        mLidarStateParam.Clock_Source = pktdata[44];

        mLidarStateParam.StandbyMode = pktdata[45];
        mLidarStateParam.PhaseLockedSwitch = pktdata[46];
        mLidarStateParam.RunningTime = mRunningTime / 60; // 转换为分钟

        mLidarStateParam.PhaseLockedAngle = PhaseAngleLocke;
        return true;
    }
    else
    {
        // 设备包未更新，返回错误
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

// 设置激光雷达参数部分

bool GetLidarData_CX128S2::setLidarRotateState(int RotateState, std::string &InfoString)
{
    // 设置激光雷达旋转状态
    if (setLidarParam())
    {
        // 验证参数有效性
        if (RotateState != 0 && RotateState != 1)
        {
            InfoString = "RotateState can only be equal to 0 or 1, please check the input parameters";
            return false;
        }
        // 设置设备数据包
        Rest_UCWP_buff[40] = 0x00;
        Rest_UCWP_buff[41] = RotateState;
        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

bool GetLidarData_CX128S2::setLidarSoureSelection(int StateValue, std::string &InfoString)
{
    // 设置激光雷达时钟源选择
    if (setLidarParam())
    {
        // 验证参数有效性
        if (StateValue != 0 && StateValue != 1)
        {
            InfoString = "StateValue can only be equal to 0 or 1, please check the input parameters";
            return false;
        }
        Rest_UCWP_buff[44] = StateValue;
        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}

bool GetLidarData_CX128S2::setLidarWorkState(int LidarState, std::string &InfoString)
{
    // 设置激光雷达工作状态
    if (setLidarParam())
    {
        // 验证参数有效性
        if (LidarState != 0 && LidarState != 1)
        {
            InfoString = "LidarState can only be equal to 0 or 1, please check the input parameters";
            return false;
        }
        Rest_UCWP_buff[45] = LidarState;
        return true;
    }
    else
    {
        InfoString = "Equipment package is not update!!!";
        return false;
    }
}