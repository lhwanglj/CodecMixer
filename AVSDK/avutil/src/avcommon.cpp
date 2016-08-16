#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "avcommon.h"

///
/*
 函数名称：
 函数功能：初始化结构体
 参    数：
 返 回 值：无返回值,void类型
 思    路：
 资    料：
 */

void bs_init( bs_t *s, void *p_data, int i_data );

/*
 该函数的作用是：从s中读出i_count位，并将其做为uint32_t类型返回
 思路:
	若i_count>0且s流并未结束，则开始或继续读取码流；
	若s当前字节中剩余位数大于等于要读取的位数i_count，则直接读取；
	若s当前字节中剩余位数小于要读取的位数i_count，则读取剩余位，进入s下一字节继续读取。
 补充:
	写入s时，i_left表示s当前字节还没被写入的位，若一个新的字节，则i_left=8；
	读取s时，i_left表示s当前字节还没被读取的位，若一个新的字节，则i_left＝8。
	注意两者的区别和联系。
 
	00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0000000
	-------- -----000 00000000 ...
	写入s时：i_left = 3
	读取s时：i_left = 5
 
 我思：
	字节流提前放在了结构体bs_s的对象bs_t里了，可能字节流不会一次性读取/分析完，而是根据需要，每次都读取几比特
	bs_s里，有专门的字段用来记录历史读取的结果,每次读取，都会在上次的读取位置上进行
	比如，100字节的流，经过若干次读取，当前位置处于中间一个字节处，前3个比特已经读取过了，此次要读取2比特
 
	00001001
	000 01 001 (已读过的 本次要读的 以后要读的 )
	i_count = 2	(计划去读2比特)
	i_left  = 5	(还有5比特未读，在本字节中)
	i_shr = s->i_left - i_count = 5 - 2 = 3
	*s->p >> i_shr，就把本次要读的比特移到了字节最右边(未读，但本次不需要的给移到了字节外，抛掉了)
	00000001
	i_mask[i_count] 即i_mask[2] 即0x03:00000011
	( *s->p >> i_shr )&i_mask[i_count]; 即00000001 & 00000011 也就是00000001 按位与 00000011
	结果是：00000001
	i_result |= ( *s->p >> i_shr )&i_mask[i_count];即i_result |=00000001 也就是 i_result =i_result | 00000001 = 00000000 00000000 00000000 00000000 | 00000001 =00000000 00000000 00000000 00000001
	i_result =
	return( i_result ); 返回的i_result是4字节长度的，是unsigned类型 sizeof(unsigned)=4
 */
int bs_read( bs_t *s, int i_count );

/*
 函数名称：
 函数功能：从s中读出1位，并将其做为uint32_t类型返回。
 函数参数：
 返 回 值：
 思    路：若s流并未结束，则读取一位
 资    料：
 毕厚杰：第145页，u(n)/u(v)，读进连续的若干比特，并将它们解释为“无符号整数”
 return i_result;	//unsigned int
 */
int bs_read1( bs_t *s );

/*
 函数名称：
 函数功能：从s中解码并读出一个语法元素值
 参    数：
 返 回 值：
 思    路：
 从s的当前位读取并计数，直至读取到1为止；
 while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )这个循环用i记录了s当前位置到1为止的0的个数，并丢弃读到的第一个1；
 返回2^i-1+bs_read(s,i)。
 例：当s字节中存放的是0001010时，1前有3个0，所以i＝3；
 返回的是：2^i-1+bs_read(s,i)即：8－1＋010＝9
 资    料：
 毕厚杰：第145页，ue(v)；无符号指数Golomb熵编码
 x264中bs.h文件部分函数解读 http://wmnmtm.blog.163.com/blog/static/382457142011724101824726/
 无符号整数指数哥伦布码编码 http://wmnmtm.blog.163.com/blog/static/38245714201172623027946/
 */
int bs_read_ue( bs_t *s );


void bs_init( bs_t *s, void *p_data, int i_data )
{
    s->p_start = (unsigned char *)p_data;		//用传入的p_data首地址初始化p_start，只记下有效数据的首地址
    s->p       = (unsigned char *)p_data;		//字节首地址，一开始用p_data初始化，每读完一个整字节，就移动到下一字节首地址
    s->p_end   = s->p + i_data;	                //尾地址，最后一个字节的首地址?
    s->i_left  = 8;				                //还没有开始读写，当前字节剩余未读取的位是8
}


int bs_read( bs_t *s, int i_count )
{
    static unsigned int i_mask[33] ={0x00,
        0x01,      0x03,      0x07,      0x0f,
        0x1f,      0x3f,      0x7f,      0xff,
        0x1ff,     0x3ff,     0x7ff,     0xfff,
        0x1fff,    0x3fff,    0x7fff,    0xffff,
        0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
        0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
        0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
        0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
    /*
			  数组中的元素用二进制表示如下：
     
			  假设：初始为0，已写入为+，已读取为-
			  
			  字节:		1		2		3		4
     00000000 00000000 00000000 00000000		下标
     
			  0x00:							  00000000		x[0]
     
			  0x01:							  00000001		x[1]
			  0x03:							  00000011		x[2]
			  0x07:							  00000111		x[3]
			  0x0f:							  00001111		x[4]
     
			  0x1f:							  00011111		x[5]
			  0x3f:							  00111111		x[6]
			  0x7f:							  01111111		x[7]
			  0xff:							  11111111		x[8]	1字节
     
     0x1ff:						 0001 11111111		x[9]
     0x3ff:						 0011 11111111		x[10]	i_mask[s->i_left]
     0x7ff:						 0111 11111111		x[11]
     0xfff:						 1111 11111111		x[12]	1.5字节
     
     0x1fff:					 00011111 11111111		x[13]
     0x3fff:					 00111111 11111111		x[14]
     0x7fff:					 01111111 11111111		x[15]
     0xffff:					 11111111 11111111		x[16]	2字节
     
		   0x1ffff:				0001 11111111 11111111		x[17]
		   0x3ffff:				0011 11111111 11111111		x[18]
		   0x7ffff:				0111 11111111 11111111		x[19]
		   0xfffff:				1111 11111111 11111111		x[20]	2.5字节
     
     0x1fffff:			00011111 11111111 11111111		x[21]
     0x3fffff:			00111111 11111111 11111111		x[22]
     0x7fffff:			01111111 11111111 11111111		x[23]
     0xffffff:			11111111 11111111 11111111		x[24]	3字节
     
     0x1ffffff:	   0001 11111111 11111111 11111111		x[25]
     0x3ffffff:	   0011 11111111 11111111 11111111		x[26]
     0x7ffffff:    0111 11111111 11111111 11111111		x[27]
     0xfffffff:    1111 11111111 11111111 11111111		x[28]	3.5字节
     
     0x1fffffff:00011111 11111111 11111111 11111111		x[29]
     0x3fffffff:00111111 11111111 11111111 11111111		x[30]
     0x7fffffff:01111111 11111111 11111111 11111111		x[31]
     0xffffffff:11111111 11111111 11111111 11111111		x[32]	4字节
     
     */
    int      i_shr;			    //
    int i_result = 0;	        //用来存放读取到的的结果 typedef unsigned   uint32_t;
    
    while( i_count > 0 )	    //要读取的比特数
    {
        if( s->p >= s->p_end )	//字节流的当前位置>=流结尾，即代表此比特流s已经读完了。
        {						//
            break;
        }
        
        if( ( i_shr = s->i_left - i_count ) >= 0 ){
            //当前字节剩余的未读位数，比要读取的位数多，或者相等
            //i_left当前字节剩余的未读位数，本次要读i_count比特，i_shr=i_left-i_count的结果如果>=0，说明要读取的都在当前字节内
            //i_shr>=0，说明要读取的比特都处于当前字节内
            //这个阶段，一次性就读完了，然后返回i_result(退出了函数)
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];//“|=”:按位或赋值，A |= B 即 A = A|B
            //|=应该在最后执行，把结果放在i_result(按位与优先级高于复合操作符|=)
            //i_mask[i_count]最右侧各位都是1,与括号中的按位与，可以把括号中的结果复制过来
            //!=,左边的i_result在这儿全是0，右侧与它按位或，还是复制结果过来了，好象好几步都多余
            /*读取后，更新结构体里的字段值*/
            s->i_left -= i_count; //即i_left = i_left - i_count，当前字节剩余的未读位数，原来的减去这次读取的
            if( s->i_left == 0 )	//如果当前字节剩余的未读位数正好是0，说明当前字节读完了，就要开始下一个字节
            {
                s->p++;				//移动指针，所以p好象是以字节为步长移动指针的
                s->i_left = 8;		//新开始的这个字节来说，当前字节剩余的未读位数，就是8比特了
            }
            return( i_result );		//可能的返回值之一为：00000000 00000000 00000000 00000001 (4字节长)
        }
        else{	/* i_shr < 0 ,跨字节的情况*/
            //这个阶段，是while的一次循环，可能还会进入下一次循环，第一次和最后一次都可能读取的非整字节，比如第一次读了3比特，中间读取了2字节(即2x8比特)，最后一次读取了1比特，然后退出while循环
            //当前字节剩余的未读位数，比要读取的位数少，比如当前字节有3位未读过，而本次要读7位
            //???对当前字节来说，要读的比特，都在最右边，所以不再移位了(移位的目的是把要读的比特放在当前字节最右)
            /* less(较少的) in the buffer than requested */
            i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;	//"-i_shr"相当于取了绝对值
            //|= 和 << 都是位操作符，优先级相同，所以从左往右顺序执行
            //举例:int|char ，其中int是4字节，char是1字节，sizeof(int|char)是4字节
            //i_left最大是8，最小是0，取值范围是[0,8]
            i_count  -= s->i_left;	//待读取的比特数，等于原i_count减去i_left，i_left是当前字节未读过的比特数，而此else阶段，i_left代表的当前字节未读的比特全被读过了，所以减它
            s->p++;	//定位到下一个新的字节
            s->i_left = 8;	//对一个新字节来说，未读过的位数当然是8，即本字节所有位都没读取过
        }
    }
    
    return( i_result );//可能的返回值之一为：00000000 00000000 00000000 00000001 (4字节长)
}

int bs_read1( bs_t *s )
{
    if( s->p < s->p_end ){
        unsigned int i_result;
        
        s->i_left--;                           //当前字节未读取的位数少了1位
        i_result = ( *s->p >> s->i_left )&0x01;//把要读的比特移到当前字节最右，然后与0x01:00000001进行逻辑与操作，因为要读的只是一个比特，这个比特不是0就是1，与0000 0001按位与就可以得知此情况
        if( s->i_left == 0 ){                   //如果当前字节剩余未读位数是0，即是说当前字节全读过了
            s->p++;			                   //指针s->p 移到下一字节
            s->i_left = 8;                     //新字节中，未读位数当然是8位
        }
        return i_result;                       //unsigned int
    }
    
    return 0;                                  //返回0应该是没有读到东西
}

int bs_read_ue( bs_t *s )
{
    int i = 0;
    while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 ){	//条件为：读到的当前比特=0，指针未越界，最多只能读32比特
        i++;
    }
    return( ( 1 << i) - 1 + bs_read( s, i ) );	
}

NALUnit_Type GetNalUnitType(NALU_t * nal)
{
    nal->Frametype = NAL_UNKNOWN;
    bs_t s;
    int frame_type = 0;
    unsigned char * buffer = nal->buf;
    
    bs_init( &s, buffer + 1  ,nal->len - 1 );
    
    if(nal->nal_unit_type == NAL_SPS){
        nal->Frametype = NAL_SPS;
    }else if(nal->nal_unit_type == NAL_PPS){
        nal->Frametype = NAL_PPS;
    }else if(nal->nal_unit_type == NAL_SEI){
        nal->Frametype = NAL_SEI;
    }else if (nal->nal_unit_type ==  NAL_SLICE_IDR ){
        /* i_first_mb */
        bs_read_ue( &s );
        /* picture type */
        frame_type =  bs_read_ue( &s );
        switch(frame_type){
            case 0: case 5: /* P */
                nal->Frametype = NAL_Ext_FRAME_P;
                break;
            case 1: case 6: /* B */
                nal->Frametype = NAL_Ext_FRAME_B;
                break;
            case 2: case 7: /* I */
                nal->Frametype = NAL_Ext_FRAME_I;
                break;
            case 3: case 8: /* SP */
                nal->Frametype = NAL_Ext_FRAME_P;
                break;
            case 4: case 9: /* SI */
                nal->Frametype = NAL_Ext_FRAME_I;
                break;
        }
    }else if (nal->nal_unit_type == NAL_SLICE){
        /* i_first_mb */
        bs_read_ue( &s );
        /* picture type */
        frame_type =  bs_read_ue( &s );
        switch(frame_type){
            case 0: case 5: /* P */
                nal->Frametype = NAL_Ext_FRAME_P;
                break;
            case 1: case 6: /* B */
                nal->Frametype = NAL_Ext_FRAME_B;
                break;
            case 2: case 7: /* I */
                nal->Frametype = NAL_Ext_FRAME_I;
                break;
            case 3: case 8: /* SP */
                nal->Frametype = NAL_Ext_FRAME_P;
                break;
            case 4: case 9: /* SI */
                nal->Frametype = NAL_Ext_FRAME_I;
                break;
        }
    }
    
    if(nal->nal_unit_type == NAL_SPS || nal->nal_unit_type == NAL_PPS || nal->nal_unit_type ==  NAL_SLICE_IDR || nal->Frametype == NAL_Ext_FRAME_I){
        nal->startcodeprefix_len = 4;
    }else{
        nal->startcodeprefix_len = 3;
    }
    
    return nal->Frametype;
}

///
NaluBuffer::NaluBuffer()
: m_max(0)
, m_len(0)
, m_buffer(NULL)
, m_firstKey(false)
{
}

NaluBuffer::NaluBuffer(unsigned int maxsize)
: m_max(maxsize)
, m_len(0)
, m_buffer(NULL)
, m_firstKey(false)
{
	if(m_max){
		m_buffer = (unsigned char *)malloc(m_max);
	}
	return;
}

NaluBuffer::~NaluBuffer()
{
	if(m_max)
	{
		free(m_buffer);
	}
	m_buffer = NULL;
}

void NaluBuffer::getBuffer(unsigned char *& buffer, unsigned int & len)
{
	buffer = m_buffer;
	len = m_len;
}

void NaluBuffer::setBuffer(unsigned char * buffer, unsigned int len, bool firstKey)
{
	if(! buffer || ! len)
	{
		return;
	}

	if(len + 4 > m_max)
	{
		m_max = len + 4;
		m_buffer = (unsigned char *)realloc(m_buffer, m_max);
	}
	memset(m_buffer, 0x00, 4);
	m_firstKey = firstKey;
	m_buffer[m_firstKey ? 3 : 2] = 0x01;
	memcpy(m_buffer + (m_firstKey ? 4 : 3), buffer, len);
	m_len = len + (m_firstKey ? 4 : 3);
}

unsigned int NaluBuffer::getNaluType()
{
    if(m_len > 4){
        unsigned char naluType = m_buffer[m_firstKey ? 4 : 3];
        return naluType & 0x1f;
    }
    
    return 0;
}

NALUnit_Type NaluBuffer::getFrameType(unsigned char * buffer, unsigned int len, bool keyFrame)
{
    if(buffer == NULL || len <= 0){
        return NAL_UNKNOWN;
    }
    NALU_t nalu_t;
    nalu_t.forbidden_bit = 0;
    nalu_t.nal_reference_idc = (buffer[0] & 0x60) >> 5;;
    nalu_t.nal_unit_type = buffer[0] & 0x1f;
    nalu_t.startcodeprefix_len = keyFrame ? 4 : 3;
    nalu_t.max_size = 1920 * 1080;
    nalu_t.len = len;
    nalu_t.buf = buffer;
    nalu_t.Frametype = NAL_UNKNOWN;
    nalu_t.lost_packets = 0;
    
    return GetNalUnitType(&nalu_t);
}

int NaluBuffer::getStartCodeSize(const unsigned char * pData,int nLen)
{
    int prefix = 0;
    if(pData[0] == 0 && pData[1] == 0){
        if(pData[2] == 1){
            prefix = 3;
        }else if(pData[2] == 0){
            if(pData[3] == 1){
                prefix = 4;
            }
        }
    }
    return prefix;
}

uint8_t convert_Byte(uint8_t * value)
{
	return value[0];
}

uint16_t convert_Word(uint8_t * value)
{
	return value[0] << 8 | value[1];
}

uint32_t convert_3Byte(uint8_t * value)
{
	return (value[0] << 16) | (value[1]) << 8 | value[2];
}

uint32_t convert_DWord(uint8_t * value)
{
	return value[0] << 24 | (value[1]) << 16 | (value[2]) << 8 | value[3];
}

double convert_8Byte(uint8_t * value)
{
	uint8_t		buf[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x40};
	for(int i=0;i<8;i++)
	{
		buf[i] = value[7-i];
	}

	return *(double *) buf;
}

//
static int SamplingFrequencyIndex[] = {
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000 ,
    7350 ,
    0    ,
    0
};

int indexSampleRate(int index)
{
//    switch(index)
//    {
//        case 0x00:
//            return 5500;
//        case 0x01:
//            return 11025;
//        case 0x02:
//            return 22050;
//        case 0x03:
//            return 44100;
//        default:
//            return -1;
//    }
    
    int size = sizeof(SamplingFrequencyIndex)/sizeof(int);
    if(index < 0 || index >= size){
        return -1;
    }
    
    return SamplingFrequencyIndex[index];
}

int indexSampleSize(int index)
{
    switch(index)
    {
        case 0:
            return 8;
        case 1:
            return 16;
        default:
            return -1;
    }
}

int indexAudioChannel(int index)
{
    switch(index)
    {
        case 0:
            return 1;
        case 1:
            return 2;
        default:
            return -1;
    }
}

int findSampInx(int sampleRate)
{
    int size = sizeof(SamplingFrequencyIndex)/sizeof(int);
    
    for(int i = 0;i < size; ++i) {
        if(SamplingFrequencyIndex[i] == sampleRate){
            return i;
        }
    }
    return -1;
}

bool makeAACSpecific(int audioObjectType, int sampleRate, int channels, unsigned char* spec_info)
{
    int sampInx = findSampInx(sampleRate);
    if(sampInx == -1){
        return false;
    }
    
    /*
     3-2、AAC sequence header，2字节：
     3-2-1、audioObjectType,5bit。结构编码类型 0=AAC main 1=AAC lc 2=AAC ssr 3=AAC LTP  一般AAC使用2，即AAC lc
     3-2-2、SamplingFrequencyIndex，4bit，音频采样率索引值 0: 96000 Hz 1: 88200 Hz 2: 64000 Hz 3: 48000 Hz 4: 44100 Hz 5: 32000 Hz 6: 24000 Hz 7: 22050 Hz 8: 16000 Hz 9: 12000 Hz 10: 11025 Hz 11: 8000 Hz 12: 7350 Hz 13: Reserved 14: Reserved  15: frequency is written explicitly  通常aac固定选中44100，即应该对应为4，但是试验结果表明，当音频采样率小于等于44100时，应该选择3，而当音频采样率为48000时，应该选择2.但是也有例外。
     3-2-3、ChannelConfiguration，4bit，音频输出声道。  对应的是音频的频道数目。单声道对应1，双声道对应2，依次类推。 0: Defined in AOT Specifc Config 1: 1 channel: front-center  2: 2 channels: front-left, front-right  3: 3 channels: front-center, front-left, front-right
     */
    
    unsigned int audio_specific_config = 0;
    audio_specific_config |= ((audioObjectType << 11) &0xF800);
    audio_specific_config |= ((sampInx << 7) &0x0780);
    audio_specific_config |= ((channels << 3) &0x78);
    audio_specific_config |= 0 & 0x07;
    
    spec_info[0] = (audio_specific_config >> 8) & 0xff;
    spec_info[1] = audio_specific_config & 0xff;
    
    return true;
}


