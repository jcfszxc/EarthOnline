#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

//码流类型
typedef enum 
{
	PT_H264 = 0x0,		//H264
	PT_H265 = 0x1, 		//H265
} PAYLOAD_TYPE_E;

//RTSP推流模块
class RTSPPush
{
protected:
	RTSPPush();

public:
	virtual ~RTSPPush();

public:
	bool Init(PAYLOAD_TYPE_E enType = PT_H265); 
	bool ReInit(PAYLOAD_TYPE_E enType = PT_H265);
	void UnInit();
	bool PushStream(int channel, uint8_t* data, unsigned int size);

	static RTSPPush* instance();

protected:
	static RTSPPush _instance;
};

#define theRTSP() (RTSPPush::instance())

#ifdef __cplusplus
}
#endif

#endif

