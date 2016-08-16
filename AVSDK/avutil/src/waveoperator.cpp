#include "waveoperator.h"


CWaveWriter::CWaveWriter(WaveFormat wf)   
:m_pFile(0)   
,m_nFileLen(0)   
,m_nOffSetFileLen(0)   
,m_nDataLen(0)   
,m_nOffSetDataLen(0)   
{   
	memcpy(&m_wf, &wf, sizeof(wf));   
}   

CWaveWriter::~CWaveWriter()   
{   
	Close();   
}   

bool CWaveWriter::IsOpened()
{
    return (m_pFile ? true : false);
}

bool CWaveWriter::Open(const char* pFileName)
{   
	Close();   
	m_pFile = fopen(pFileName, "wb");   
	if(!m_pFile)   
		return false;   

    WriteHeader();

	return true;   
}   

void CWaveWriter::Close()   
{   
	if(m_pFile)   
	{   
        WriteTail();
		fclose(m_pFile);   
		m_pFile = 0;   
	}   
}   

int CWaveWriter::WriteHeader()   
{   
	fwrite("RIFF", 1, 4, m_pFile);   
	fwrite(&m_nFileLen, 4, 1, m_pFile);
	fwrite("WAVE", 1, 4, m_pFile);   
	fwrite("fmt ", 1, 4, m_pFile);     
	int nTransBytes = 0x00000010;
	fwrite(&nTransBytes, 4, 1, m_pFile);   
	fwrite(&m_wf.nFormatag, 2, 1, m_pFile);   
	fwrite(&m_wf.nChannels, 2, 1, m_pFile);   
	fwrite(&m_wf.nSamplerate, 4, 1, m_pFile);   
	fwrite(&m_wf.nAvgBytesRate, 4, 1, m_pFile);   
	fwrite(&m_wf.nblockalign, 2, 1, m_pFile);   
	fwrite(&m_wf.nBitsPerSample, 2, 1, m_pFile);   
	fwrite("data", 1, 4, m_pFile);   
	fwrite(&m_nDataLen, 4, 1, m_pFile);

	m_nFileLen = 0x2C;   
	m_nDataLen = 0x00;   
	m_nOffSetFileLen = 0x04;   
	m_nOffSetDataLen = 0x28;   

	return 0x2C;   
}   

int CWaveWriter::WriteData(unsigned char* pData, int nLen)   
{   
	fwrite(pData, 1, nLen, m_pFile);   
	m_nDataLen += nLen;   
	m_nFileLen += nLen;   
	return nLen;   
}   

int CWaveWriter::WriteTail()   
{   
	fseek(m_pFile, m_nOffSetFileLen, SEEK_SET);   
	fwrite(&m_nFileLen, 4, 1, m_pFile);   
	fseek(m_pFile, m_nOffSetDataLen, SEEK_SET);   
	fwrite(&m_nDataLen, 4, 1, m_pFile);       
	return 0;   
}   

////////////////////////////////////////////    


CWaveReader::CWaveReader()   
:m_pFile(0)   
{   
	memset(&m_WaveFormat, 0, sizeof(m_WaveFormat));   
}    

CWaveReader::~CWaveReader()   
{   
	Close();   
}   

bool CWaveReader::Open(const char* pFileName)
{   
	Close();   
	m_pFile = fopen(pFileName, "rb");   
	if( !m_pFile )   
		return false;   

	if( !ReadHeader() )
		return false;

	return true;      
}   

void CWaveReader::Close()   
{   
	if(m_pFile)   
	{   
		fclose(m_pFile);   
		m_pFile = 0;   
	}   
}   

bool CWaveReader::ReadHeader()   
{   
	if(!m_pFile)   
		return false;   

	int Error = 0;   
	do   
	{   
		char data[5] = { 0 };   

		fread(data, 4, 1, m_pFile);   
		if(strcmp(data, "RIFF") != 0)   
		{   
			Error = 1;   
			break;   
		}   

		fseek(m_pFile, 4, SEEK_CUR);   
		memset(data, 0, sizeof(data));   
		fread(data, 4, 1, m_pFile);   
		if(strcmp(data, "WAVE") != 0)   
		{   
			Error = 1;   
			break;   
		}   

		memset(data, 0, sizeof(data));   
		fread(data, 4, 1, m_pFile);   
		if(strcmp(data, "fmt ") != 0)   
		{   
			Error = 1;   
			break;   
		}   

		memset(data, 0, sizeof(data)); 
		fread(data, 4, 1, m_pFile);   

		int nFmtSize =  data[3] << 24;
		nFmtSize	+=  data[2] << 16;
		nFmtSize    +=  data[1] << 8;
		nFmtSize    +=  data[0];

		if(nFmtSize >= 16)
		{
			if( fread(&m_WaveFormat, 1, sizeof(m_WaveFormat), m_pFile)    
				!= sizeof(m_WaveFormat) )   
			{   
				Error = 1;   
				break;   
			}

			if(nFmtSize == 18)
			{
				memset(data, 0, sizeof(data)); 
				fread(data, 2, 1, m_pFile);   
				short  cbSize = data[1] << 8;
				cbSize += data[0];
				fseek(m_pFile, cbSize, SEEK_CUR);   
			}
		}
		else 
		{
			return false;
		}

		memset(data, 0, sizeof(data));   
		bool bFindData = false;
		do
		{
			fread(data, 4, 1, m_pFile);   
			if(strcmp(data, "data") == 0)   
			{   
				bFindData = true;
				break;   
			} 
		}while(!feof(m_pFile));

		if(bFindData)
		{
			fread(&m_nDataLen, 4, 1, m_pFile);          
		}
		else
		{
			Error = 1;   
		}
	}while(0);   


	ftell(m_pFile);
	if(0 == Error)   
		return true;   
	else
		fseek(m_pFile, 0, 0);

	return false;   
}   

int CWaveReader::ReadData(unsigned char* pData, int nLen)   
{      
	if(m_pFile)   
		return (int)fread(pData, 1, nLen, m_pFile);

	return -1;   
}   

bool CWaveReader::GetFormat(WaveFormat* pWaveFormat)   
{   
	memcpy(pWaveFormat, &m_WaveFormat, sizeof(m_WaveFormat));   
	return true;
}   

FILE* CWaveReader::Handle()
{
    return m_pFile;
}
