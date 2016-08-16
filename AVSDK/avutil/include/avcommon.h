#ifndef __AVCOMMON_H_

#define __AVCOMMON_H_


#include <stdint.h>
    
#define NALU_TYPE_SLICE 1
#define NALU_TYPE_DPA 2
#define NALU_TYPE_DPB 3
#define NALU_TYPE_DPC 4
#define NALU_TYPE_IDR 5 //I帧
#define NALU_TYPE_SEI 6
#define NALU_TYPE_SPS 7
#define NALU_TYPE_PPS 8
#define NALU_TYPE_AUD 9 //访问分隔符
#define NALU_TYPE_EOSEQ 10
#define NALU_TYPE_EOSTREAM 11
#define NALU_TYPE_FILL 12

#define NALU_FRAME_I 15
#define NALU_FRAME_P 16
#define NALU_FRAME_B 17
    
    typedef struct{
        unsigned char type;
        unsigned int nTimeStamp;
        unsigned int nStreamID;
        unsigned char * data;
        unsigned int len;
        bool haveAudio;
        bool haveVideo;
        
        unsigned int totalSize;
        unsigned int totalTime;
        float percent;
    }FileTag;
    
	typedef struct {
		uint8_t configurationVersion;
		uint8_t avcProfileIndication;
		uint8_t profile_compatibility;
		uint8_t avcLevelIndication;
		uint8_t lengthSizeMinusOne;
		uint8_t numOfSequenceParameterSets;
		uint16_t sequenceParameterSetLength;
		uint8_t * sequenceParameterSetNALUnits;
		uint8_t numOfPictureParameterSets;
		uint16_t pictureParameterSetLength;
		uint8_t * pictureParameterSetNALUits;
	} AVCDecoderConfigurationRecord;


	typedef struct {
		uint8_t audioObjectType : 5;
		uint8_t samplingFrequencyIndex : 4;
		uint8_t channelConfiguration : 4;

		typedef struct{
			uint8_t frameLengthFlag : 1;
			uint8_t dependsOnCoreCoder : 1;
			uint8_t extensionFlag : 1;
		}GASpecificConfig;
		GASpecificConfig gaSpecificConfig;
	}FLV_AudioSpecificConfig;

#pragma pack(1)
	struct AACADST{
		unsigned short int syncWord:12;
		unsigned char ID:1;
		unsigned char layer:2;
		unsigned char protection_Absent:1;
		unsigned char profile:2;
		unsigned char Sampling_frequency_Index:4;
		unsigned char private_bit:1;
		unsigned char channel_Configuration:3;
		unsigned char original_copy:1;
		unsigned char home:1;
		unsigned char copyright_identiflcation_bit:1;
		unsigned char copyright_identiflcation_start:1;
		unsigned short int aac_frame_length:13;
		unsigned short int adts_buffer_fullness:11;
		unsigned char number_of_raw_data_blocks_in_frame:2;
		//unsigned short int CRC:16;
	};
#pragma pack()

	int indexSampleRate(int index);
	int indexSampleSize(int index);
	int indexAudioChannel(int index);

	uint8_t convert_Byte(uint8_t * value);
	uint16_t convert_Word(uint8_t * value);
	uint32_t convert_3Byte(uint8_t * value);
	uint32_t convert_DWord(uint8_t * value);
	double convert_8Byte(uint8_t * value);

	///
#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#define Swap32(l) (((l) >> 24) | \
	(((l) & 0x00ff0000) >> 8)  | \
	(((l) & 0x0000ff00) << 8)  | \
	((l) << 24))
#define Swap64(ll) (((ll) >> 56) |\
	(((ll) & 0x00ff000000000000) >> 40) |\
	(((ll) & 0x0000ff0000000000) >> 24) |\
	(((ll) & 0x000000ff00000000) >> 8)	|\
	(((ll) & 0x00000000ff000000) << 8)	|\
	(((ll) & 0x0000000000ff0000) << 24) |\
	(((ll) & 0x000000000000ff00) << 40) |\
	(((ll) << 56)))
	///

    
    //nal类型
    typedef enum _NALUnit_Type
    {
        NAL_UNKNOWN     = 0,
        NAL_SLICE       = 1,
        NAL_SLICE_DPA   = 2,
        NAL_SLICE_DPB   = 3,
        NAL_SLICE_DPC   = 4,
        NAL_SLICE_IDR   = 5,    /* ref_idc != 0 */
        NAL_SEI         = 6,    /* ref_idc == 0 */
        NAL_SPS         = 7,
        NAL_PPS         = 8,
        /* ref_idc == 0 for 6,9,10,11,12 */
        NAL_Ext_FRAME_I  = 15,
        NAL_Ext_FRAME_P  = 16,
        NAL_Ext_FRAME_B  = 17,
    }NALUnit_Type;
    
    typedef struct Tag_NALU_t
    {
        unsigned char forbidden_bit;           //! Should always be FALSE
        unsigned char nal_reference_idc;       //! NALU_PRIORITY_xxxx
        unsigned char nal_unit_type;           //! NALU_TYPE_xxxx
        unsigned int  startcodeprefix_len;      //! 前缀字节数
        unsigned int  len;                     //! 包含nal 头的nal 长度，从第一个00000001到下一个000000001的长度
        unsigned int  max_size;                //! 最多一个nal 的长度
        unsigned char * buf;                   //! 包含nal 头的nal 数据
        NALUnit_Type Frametype;               //! 帧类型
        unsigned int  lost_packets;            //! 预留
    } NALU_t;
    

    //读取字节结构体
    typedef struct Tag_bs_t
    {
        unsigned char *p_start;	               //缓冲区首地址(这个开始是最低地址)
        unsigned char *p;			           //缓冲区当前的读写指针 当前字节的地址，这个会不断的++，每次++，进入一个新的字节
        unsigned char *p_end;		           //缓冲区尾地址		//typedef unsigned char   uint8_t;
        int     i_left;				           // p所指字节当前还有多少 “位” 可读写 count number of available(可用的)位
    }bs_t;
    
    
    class NaluBuffer
    {
    public:
		NaluBuffer();
		NaluBuffer(unsigned int maxsize);
		virtual ~NaluBuffer();

		void getBuffer(unsigned char *& buffer, unsigned int & len);
		void setBuffer(unsigned char * buffer, unsigned int len, bool firstKey = false);
        unsigned int getNaluType();
        static NALUnit_Type getFrameType(unsigned char * buffer, unsigned int len, bool keyFrame);
        static int getStartCodeSize(const unsigned char * pData,int nLen);
        
    private:
        unsigned int m_max;
		unsigned int m_len;
		unsigned char * m_buffer;

		bool m_firstKey;
	};
    
 
    bool makeAACSpecific(int audioObjectType, int sampleRate, int channels, unsigned char* spec_info);


#endif