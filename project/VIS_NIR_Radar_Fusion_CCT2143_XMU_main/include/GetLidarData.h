/**
 * GetLidarData.h - 激光雷达数据获取与处理基类
 *
 * 本头文件定义了激光雷达数据获取和处理的跨平台基类结构。
 * 主要功能包括：
 * 1. 网络通信 - 建立UDP Socket连接接收雷达数据和设备参数，支持Windows与Linux平台
 * 2. 数据结构 - 定义雷达点云数据结构、设备状态参数、过滤参数等基础类型
 * 3. 参数配置 - 提供完整的雷达参数设置接口（IP地址、端口、旋转速度等）
 * 4. 设备控制 - 通过命令包实现对雷达设备的状态控制（工作模式、电机控制等）
 * 5. 数据过滤 - 支持基于多维条件（角度、距离、强度、通道等）的点云数据过滤
 * 6. 时间同步 - 管理雷达数据的时间戳和帧同步，支持多种时间源（GPS、PTP、NTP等）
 * 7. 回调机制 - 通过函数回调实现数据点的异步处理和传递
 * 8. 性能优化 - 预计算三角函数值，使用智能指针等提高数据处理效率
 *
 * 该基类设计为抽象类，要求子类实现特定雷达型号的数据解析和运行逻辑。
 * 支持多种雷达型号（如CX6S3、LS系列等）的参数设置和控制。
 */

#pragma once

// 包含必要的头文件
#include <iostream>	  // 用于标准输入输出
#include <vector>	  // 用于向量容器
#include <thread>	  // 用于线程操作
#include <queue>	  // 用于队列容器
#include <mutex>	  // 用于互斥锁
#include <cmath>	  // 用于数学运算
#include <chrono>	  // 用于时间相关操作
#include <cstring>	  // 用于字符串操作
#include <functional> // 用于函数对象
#include <string>	  // 用于字符串类
#include <regex>	  // 用于正则表达式
#include <memory>	  // 用于智能指针

#include <sstream> // 用于字符串流操作
#include <iomanip> // 用于I/O格式化

// 根据平台包含不同的头文件
#ifdef LINUX
// Linux系统头文件
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h> // 用于套接字操作
#include <arpa/inet.h>	// 用于IP地址转换

#include <sys/epoll.h>	  // 用于I/O多路复用
#include <fcntl.h>		  // 用于文件控制
#include <errno.h>		  // 用于错误处理
typedef uint64_t u_int64; // 定义64位无符号整型
#else
// Windows系统头文件
#include <WinSock2.h>			   // 用于套接字操作
#include <WS2tcpip.h>			   // 用于TCP/IP套接字扩展
#pragma comment(lib, "ws2_32.lib") // 链接Winsock库

// 定义Windows下的Socket上下文结构体
struct SocketContext
{
	SOCKET socket;			  // 套接字
	char buffer[1212];		  // 数据缓冲区
	WSAOVERLAPPED overlapped; // 重叠I/O结构
	WSABUF dataBuf;			  // 数据缓冲区描述
	SOCKADDR_IN remoteAddr;	  // 存储数据来源IP地址
};

typedef unsigned __int64 u_int64; // 定义64位无符号整型

// 时间转换函数重定向
#define timegm _mkgmtime
#endif

// 常量定义
#define PI 3.1415926		 // 圆周率
#define ConversionUnit 100.0 // 单位转换：将厘米转换为米

// 三维点结构体定义
typedef struct _Point_XYZ
{
	float x1 = 0.0; // X1坐标
	float y1 = 0.0; // Y1坐标
	float z1 = 0.0; // Z1坐标

	float x2 = 0.0; // X2坐标
	float y2 = 0.0; // Y2坐标
	float z2 = 0.0; // Z2坐标

	float H_angle = 0.0; // 水平角度
	float V_angle = 0.0; // 垂直角度
} m_PointXYZ;

// 雷达数据结构体定义
typedef struct _MuchLidarData
{
	float X = 0.0;				 // X坐标
	float Y = 0.0;				 // Y坐标
	float Z = 0.0;				 // Z坐标
	int ID = 0;					 // 点ID
	float H_angle = 0.0;		 // 水平角度
	float V_angle = 0.0;		 // 垂直角度
	float Distance = 0.0;		 // 距离
	int Intensity = 0;			 // 强度值
	u_int64 Mtimestamp_nsce = 0; // 时间戳(纳秒)
} MuchLidarData;

// UTC时间结构体定义
typedef struct _UTC_Time
{
	int year = 2000; // 年
	int month = 0;	 // 月
	int day = 0;	 // 日
	int hour = 0;	 // 时
	int minute = 0;	 // 分
	int second = 0;	 // 秒
} UTC_Time;

// 雷达状态参数结构体定义
typedef struct _LidarStateParam
{
	std::string LidarIP = "-1,-1,-1,-1";		  // 雷达IP
	std::string ComputerIP = "-1,-1,-1,-1";		  // 雷达目标IP（计算机IP）
	std::string NtpIP = "-1,-1,-1,-1";			  // 雷达NTP IP
	std::string GatewayIP = "-1,-1,-1,-1";		  // 雷达网关IP
	std::string SubnetMaskIP = "-1,-1,-1,-1";	  // 雷达子网掩码
	std::string MacAddress = "-1,-1,-1,-1,-1,-1"; // 雷达MAC地址	50 3E 7C ** ** **
	int DataPort = -1;							  // 雷达数据包端口
	int DevPort = -1;							  // 雷达设备包端口

	float ReceiverTemperature = -1.0; // 接收板温度    单位:°
	float ReceiverHighVoltage = -1.0; // 接收板高压    单位:伏特
	float MotorSpeed = -1;			  // 电机转速      单位:转/分钟
	int FrameRateMode = -1;			  // 帧率模式      值: 0; 1; 2

	int PTP_State = -1;			  // PTP状态:      1:丢失;    0:未丢失
	int GPS_State = -1;			  // GPS状态:      1:丢失;    0:未丢失
	int PPS_State = -1;			  // PPS状态:      1:丢失;    0:未丢失
	int StandbyMode = -1;		  // 待机模式      0:正常     1:待机
	int Clock_Source = -1;		  // 时钟源:       0:GPS;     1:PTP_L2; 2:NTP; 3:PTP_UDPV4
	int PhaseLockedSwitch = -1;	  // 相位锁定开关: 0:关闭;    1:开启
	float PhaseLockedAngle = -1;  // 相位锁定角度: 相位锁定角度值
	int PhaseLockedState = -1;	  // 相位锁定状态: 0:未锁定;  1:已锁定
	std::string FaultCode = "-1"; // 故障代码      每一位表示一个故障状态
	double RunningTime = -1;	  // 运行时间      单位: 小时
} LidarStateParam;

// 雷达过滤参数显示结构体定义
typedef struct _LidaFilterParamDisplay
{
	float mMin_HanleValue; // 最小水平角度值，单位:°
	float mMax_HanleValue; // 最大水平角度值，单位:°

	float mMin_VanleValue; // 最小垂直角度值，单位:°
	float mMax_VanleValue; // 最大垂直角度值，单位:°

	float mMin_Distance; // 最小距离，单位:米
	float mMax_Distance; // 最大距离，单位:米

	float mMin_Intensity;			 // 最小强度值
	float mMax_Intensity;			 // 最大强度值
	std::vector<int> mChannelVector; // 通道向量，1表示显示，0表示过滤

	// 默认构造函数，初始化所有参数
	_LidaFilterParamDisplay()
	{
		mMin_HanleValue = -360; // 设置最小水平角度为-360度
		mMax_HanleValue = 360;	// 设置最大水平角度为360度

		mMin_VanleValue = -180; // 设置最小垂直角度为-180度
		mMax_VanleValue = 180;	// 设置最大垂直角度为180度

		mMin_Distance = 0;	  // 设置最小距离为0米
		mMax_Distance = 1000; // 设置最大距离为1000米

		mMin_Intensity = 0;	  // 设置最小强度为0
		mMax_Intensity = 255; // 设置最大强度为255

		mChannelVector.resize(256); // 调整通道向量大小为256
		for (long int FF = 0; FF < 256; FF++)
		{
			mChannelVector[FF] = 1; // 初始化所有通道为显示状态
		}
	}

	// 重载赋值运算符
	_LidaFilterParamDisplay &operator=(const _LidaFilterParamDisplay &obj)
	{
		if (&obj == this) // 检查自赋值
		{
			return *this;
		};

		// 复制所有成员变量
		mMin_HanleValue = obj.mMin_HanleValue;
		mMax_HanleValue = obj.mMax_HanleValue;

		mMin_VanleValue = obj.mMin_VanleValue;
		mMax_VanleValue = obj.mMax_VanleValue;

		mMin_Distance = obj.mMin_Distance;
		mMax_Distance = obj.mMax_Distance;

		mMin_Intensity = obj.mMin_Intensity;
		mMax_Intensity = obj.mMax_Intensity;

		// 复制通道向量
		for (size_t FF = 0; FF < obj.mChannelVector.size(); FF++)
		{
			mChannelVector[FF] = obj.mChannelVector[FF];
		}

		// 如果源对象通道向量小于256，将剩余通道设为显示状态
		for (long int FF = obj.mChannelVector.size(); FF < 256; FF++)
		{
			mChannelVector[FF] = 1;
		}
		return *this;
	}

} LidaFilterParamDisplay;

// 定义回调函数类型，用于处理雷达数据
typedef std::function<void(std::shared_ptr<std::vector<MuchLidarData>>, int, std::string)> FunDataPrt;

// 获取雷达数据的基类
class GetLidarData
{
public:
	GetLidarData();	   // 构造函数
	~GetLidarData();   // 析构函数
	int setIndex = -1; // 设置索引

	bool isQuit = false;		 // 雷达退出标志
	bool isFrameOK = false;		 // 帧是否正常
	int isDormant = 0;			 // 雷达是否处于低功耗模式，0:正常模式，1:低功耗模式
	bool isFrames = true;		 // 帧同步判断，true:正常
	bool isHighVoltage = true;	 // 电压是否正常，true:正常
	bool islidarDevCome = false; // 判断是否获取雷达设备包

	double cosAngleValue[360000]; // 预计算角度的余弦值
	double sinAngleValue[360000]; // 预计算角度的正弦值

	std::string mDataInfoString; // 数据包的消息反馈
	std::string mDevInfoString;	 // 设备包的消息反馈
	bool isSuccessfulFlag;		 // 是否成功获取数据

	LidaFilterParamDisplay mLidaFilterParamDisplayValue; // 雷达过滤参数显示值

	/*
		功能：获取数据包状态
		参数：无
		返回值：std::string：返回数据包的状态信息
	*/
	std::string getDataPacketState();

	/*
		功能：获取设备包状态
		参数：无
		返回值：std::string：返回设备包的状态信息
	*/
	std::string getDevPacketState();

	/*
	功能：获取雷达参数状态
	参数：LidarStateParam: 雷达状态参数反馈；InfoString:消息反馈
	返回值：true, 操作成功；false: 操作失败，查看InfoString
	*/
	virtual bool getLidarParamState(LidarStateParam &mLidarStateParam, std::string &InfoString);

	/*
		功能：初始化数据包端口，设备包端口和目标IP地址
		参数1：mDataPort      数据包端口      2368（默认）
		参数2：mDevPort       设备包端口      2369（默认）
		参数3：mDestIP        目标IP         "192.168.1.102"(默认)
		参数4：mLidarIP       雷达IP         "192.168.1.200"(默认)
		参数5：mGroupIp       组播IP         "224.1.1.102"(默认)
		返回值：无
	*/
	void setPortAndIP(uint16_t mDataPort = 2368, uint16_t mDevPort = 2369, std::string mDestIP = "192.168.1.102", std::string mLidarIP = "192.168.1.200", std::string mGroupIp = "226.1.1.102");

	/*
		功能：设置雷达固定IP、雷达IP、目标IP、数据包端口和设备端口
		参数1：mFixedLidarIP   雷达固定IP      "10.10.10.254"   (默认)
		参数2：mLidarIP        雷达IP          "192.168.1.200"  (默认)
		参数3：mDestIP         目标IP          "192.168.1.102"  (默认)
		参数4：mDataPort       数据包端口       2368            (默认)
		参数5：mDevPort        设备包端口       2369            (默认)
		返回值：无
	**************  目前仅适用于CX6S3  *********/
	virtual void setChangeLidarIP(std::string mFixedLidarIP = "10.10.10.254", std::string mLidarIP = "192.168.1.200", std::string mDestIP = "192.168.1.102", uint16_t mDataPort = 2368, uint16_t mDevPort = 2369);

	/*
		功能：传输回调函数，获取雷达数据
		参数1：FunDataPrt*     引入回调函数指针，获取雷达数据
		返回值：无
	*/
	void setCallbackFunction(FunDataPrt *); // 设置数据点回调函数

	/*
		功能：开始解析雷达数据
		参数：无
		返回值：无
	*/
	void LidarStart();

	/*
	功能：停止解析雷达数据
	参数：无
	返回值：无
	*/
	void LidarStop();

	/*
		功能：开始网络数据包的离线数据解析
		参数：无
		返回值：无
	*/
	void LidarOfflineDataStar();

	/*
	功能：获取一帧雷达数据
	参数：preFrameData: 获取一帧点云数据; Info:信息
	返回值：true，操作成功；false: 操作失败
	*/
	bool getLidarPerFrameDate(std::shared_ptr<std::vector<MuchLidarData>> &preFrameData, std::string &Info);

	virtual void LidarRun() = 0;						  // 纯虚函数，子类必须实现的雷达运行函数
	void CollectionDataArrive(void *pData, uint16_t len); // 在此传输UDP数据

	int NegativeToPositive(float); // 负数转换为正数

	std::string ucharToBinaryStr(const unsigned char); // 无符号字符转换为二进制字符串

public:
	// #pragma region //设置雷达参数
	unsigned char dataDev[1206];		// 接收设备包
	unsigned char Rest_UCWP_buff[1206]; // 修改配置包
	std::string ip_sa;					// 发送雷达IP
	int m_SpeedValue = 0;				// 设置雷达旋转速度
	bool is_speedFlag = false;			// 是否启用旋转速度检测，true:启用
	bool is_speedNormal = true;			// 旋转速度是否正常，true:正常

	// 整数转十六进制字符串函数
	std::string IntToHex(int value)
	{
		std::ostringstream ss;
		ss << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << value;
		return ss.str();
	}
	/*
	功能：修改雷达旋转速度
	参数：SpeedValue  旋转速度值: 支持300，600，1200; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败

	注意：LS雷达不适用
	*/
	virtual bool setLidarRotateSpeed(int SpeedValue, std::string &InfoString); // 设置雷达旋转速度

	/*
	功能：修改雷达IP
	参数：IPString 输入要修改的IPv4地址，如"192.168.1.200"; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setLidarIP(std::string IPString, std::string &InfoString); // 设置雷达IP

	/*
	功能：修改雷达目标IP（计算机IP）
	参数：IPString 输入要修改的IPv4地址，如"192.168.1.102"; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setComputerIP(std::string IPString, std::string &InfoString); // 设置计算机IP

	/*
	功能：修改雷达NTP IP
	参数：IPString 输入要修改的IPv4地址，如"192.168.1.102"; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setNTP_IP(std::string IPString, std::string &InfoString); // 设置NTP IP

	/*
	功能：修改雷达网关IP（网关IP）
	参数：IPString 输入要修改的IPv4地址，如"192.168.1.254"; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setGatewayIP(std::string IPString, std::string &InfoString); // 设置网关IP

	/*
	功能：修改雷达子网掩码IP（子网掩码IP）
	参数：IPString 输入要修改的IPv4地址，如"255.255.255.0"; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setSubnetMaskIP(std::string IPString, std::string &InfoString); // 设置子网掩码IP

	/*
	功能：修改雷达数据包端口
	参数：PortNum, 输入雷达数据端口，默认可输入0~65536（建议大于2000的值）; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setDataPort(int PortNum, std::string &InfoString); // 设置数据包端口

	/*
	功能：修改设备包端口
	参数：PortNum, 输入雷达数据端口，默认可输入0~65536（建议大于2000的值）; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setDevPort(int PortNum, std::string &InfoString); // 设置设备包端口

	/*
	功能：修改雷达电机状态
	参数：StateValue, 输入雷达电机的模式，0表示：运行（默认），1：静止; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setLidarRotateState(int StateValue, std::string &InfoString); // 设置雷达电机状态：运行或静止

	/*
	功能：修改雷达时间源选择
	参数：StateValue: 输入源选择，默认，0:GPS；1: PTP_L2; 2:NTP; 3:PTP_UDPV4; InfoString:消息反馈
	返回值：true：操作成功；false：操作失败
	*/
	virtual bool setLidarSoureSelection(int StateValue, std::string &InfoString); // 设置时间源

	/*
	功能：修改雷达低功耗模式，即雷达只发送设备包而不发送数据包
	参数：StateValue: 输入模式，默认，0:正常工作模式，1:低功耗模式; InfoString:消息反馈
	返回值：true：操作成功；false：操作失败
	*/
	virtual bool setLidarWorkState(int StateValue, std::string &InfoString); // 雷达状态：是否处于低功耗模式

	/*
	功能：修改雷达帧率模式
	参数：StateValue: 输入模式，默认，0:正常帧率；1: 50%帧率; 2: 25%帧率; InfoString:消息反馈
	返回值：true：操作成功；false：操作失败

	注意：仅适用于LS雷达
	*/
	virtual bool setFrameRateMode(int StateValue, std::string &InfoString); // 设置雷达帧率模式

	/*
	功能：修改雷达相位锁定开关
	参数：StateValue: 输入模式，默认，0:关闭; 1: 打开; InfoString:消息反馈
	返回值：true：操作成功；false：操作失败

	注意：仅适用于LS雷达
	*/
	virtual bool setPhaseLockedSwitch(int StateValue, std::string &InfoString); // 设置雷达相位锁定开关

	/*
	功能：修改雷达索引参数值
	参数：ControlValue: 索引 ChangeValue: 变更值; InfoString:消息反馈
	返回值：true，操作成功；false: 操作失败
	*/
	virtual bool setLidarIndexParamValue(int ControlValue, int ChangeValue, std::string &InfoString);

	/*
	功能：发送UDP包，雷达数据
	参数：无
	返回值：true：操作成功；false：操作失败
	*/
	virtual bool sendPackUDP(); // 设置UDP包

	/*
	功能：第二IP控制开关
	参数：0表示启用，1表示禁用; InfoString:消息反馈
	返回值：true：操作成功；false：操作失败
	*/
	virtual bool sendSecondIP(int switchIP, std::string &InfoString); // 第二IP控制开关仅适用于cx6s3

	/*
	功能：指示灯控制
	参数：1表示启用，0表示禁用; InfoString:消息反馈
	返回值：true：操作成功；false：操作失败
	*/
	virtual bool sendSecondCueLight(int switchIP, std::string &InfoString); // 仅适用于CX6S3

	bool setLidarParam();																  // 设置包
	bool sendPacketToLidar(unsigned char *packet, const char *ip_data, u_short portdata); // 通过套接字发送包
	// #pragma endregion

	// #pragma region//雷达过滤显示设置
	/*
	功能：显示过滤的水平角度
	参数：LidaFilterParamDisplay : 输入雷达过滤显示参数;
	返回值：true：操作成功；false：操作失败
	*/
	virtual bool setLidarFilterdisplay(LidaFilterParamDisplay mLidaFilterParamDisplay);

	virtual bool isPointFilter(const MuchLidarData mPoint); // 返回值：true：过滤；false：保留
															// #pragma endregion

public:
	u_int64 timestamp_nsce;		  // 数据包中小于1秒的时间戳(单位：纳秒)
	u_int64 allTimestamp;		  // 保存数据包中的时间戳
	u_int64 lastAllTimestamp = 0; // 保存上一个数据包的时间戳
	double PointIntervalT_ns;	  // 数据包中每个点的时间间隔，单位是纳秒
	int m_DistanceIsNotZero = 0;

protected:
	std::queue<unsigned char *> allDataValue; // 数据缓存队列
	std::mutex m_mutex;						  // 互斥锁

	int m_messageCount = 0;
	std::shared_ptr<std::vector<MuchLidarData>> LidarPerFrameDatePrt_Get;  // 获取数据
	std::shared_ptr<std::vector<MuchLidarData>> LidarPerFrameDatePrt_Send; // 发送数据

	UTC_Time m_UTC_Time;								   // 获取设备包的GPS信息
	void clearQueue(std::queue<unsigned char *> &m_queue); // 清除队列
	void sendLidarData();								   // 打包并发送数据
	FunDataPrt *callback = NULL;						   // 回调函数指针--回调数据包
	std::string time_service_mode = "gps";				   // 计时方法

	void messFunction(std::string strValue, int gValue);
	// 检查源IP是否匹配
	bool checkDefaultIP(std::vector<std::string> m_DefaultIP, std::string &InfoString);
	// 检查目标IP是否匹配
	bool checkDestIP(std::vector<std::string> m_DestIP, std::string &InfoString);

	// 一段时间后开始转速判断
	void sleepTime();
	// 开始转速判断
	void startSleepThread();

private:
	std::shared_ptr<std::vector<MuchLidarData>> LidarPerFrameDatePer;
	bool isSendUDP = true;

	int dataPort = 2368;					  // 数据包端口
	int devPort = 2369;						  // 设备包端口
	std::string computerIP = "192.168.1.102"; // 雷达目标IP（计算机IP）
	std::string lidarIP = "192.168.1.200";	  // 雷达自身IP
	std::string groupIp = "226.1.1.102";	  // 定义组播IP

#ifdef LINUX
	int fd_;

	// 启动线程获取设备包
	int LinuxSockCreate();
#else
	SOCKET m_SockData;
	SOCKET m_SockDev;

	// 启动线程获取数据包
	void WindowSockCreate();
#endif
};