 /*
 * FILE: BitImage.h
 *
 * DESCRIPTION:   A  class to realize a Bit image whose pixel is bi-value: 1/0 (true or false)
 *
 * CREATED BY: He wenfeng, 2005/4/1
 *
 * HISTORY: 
 *
 * Copyright (c) 2004 Archosaur Studio, All Rights Reserved.
 */

#ifndef _BITIMAGE_H_
#define _BITIMAGE_H_

#include <stdio.h>
#include <memory.h>

#define UCHAR_BITWISE_NOT(x)  ( 0xFF ^ (x) )

// Version Info (DWORD)

// Current Version
#define BITIMAGE_VER 0x00000001	

// Old Version


// class laid under the namespace NPCMoveMap
namespace NPCMoveMap
{

typedef unsigned int DWORD;
typedef unsigned char UCHAR;


class CBitImage
{

public:	
	CBitImage() { m_BitImage = NULL; }
	virtual ~CBitImage() { Release(); }
	
	void Release() 
	{ 
		if(m_BitImage) 
		{
			delete [] m_BitImage; 
			m_BitImage=NULL;
		}
	}

	template<class T>
	void Init(T* pImage, int iWidth, int iLength, const T& tSetBitValue, float fPixelSize = 1.0f)
	{
		// Firstly, we release myself
		Release();

		m_iWidth=iWidth >> 3;
		//int iLastBitWidth=iWidth - (m_iWidth << 3); 
		int iLastBitWidth=iWidth & 0x7; 
		if(iLastBitWidth!=0)
			m_iWidth++;

		m_iLength=iLength;

		m_fPixelSize = fPixelSize;
		m_iImageWidth = iWidth;
		m_iImageLength= iLength;

		int iImageSize = m_iWidth * m_iLength;
		m_BitImage = new UCHAR[iImageSize];
		memset(m_BitImage, 0, iImageSize);
		
		for(int i=0; i<iLength; i++)
			for(int j=0; j< iWidth; j++)
			{
				int jInChar= j >> 3;
				//int Shift = j - ( jInChar << 3 );
				int Shift = j & 0x7 ;
				bool bSet=(pImage[i*iWidth+j]==tSetBitValue)?true:false;
				SetUCharBit(m_BitImage[i*m_iWidth+ jInChar], Shift, bSet);
			}
		
	}

	inline void InitZero(int iWidth, int iLength, float fPixelSize = 1.0f);
	inline void InitNoneZero(int iWidth, int iLength, float fPixelSize = 1.0f);
	
	bool GetPixel(int u, int v)
	{
		int uInChar = u >> 3;
		//int shift = u - (uInChar << 3);
		int shift = u & 0x7;
		if( m_BitImage[v*m_iWidth + uInChar] & (UCHAR)(1 << shift) )
			return true;
		else
			return false;
	}

	void GetImageSize(int& width, int& length)
	{
		width = m_iImageWidth;
		length = m_iImageLength;
	}

	float GetPixelSize()
	{
		return m_fPixelSize;
	}
	
	// Load and Save, using FILE
	inline bool Save( FILE *pFileToSave );
	inline bool Load( FILE *pFileToLoad );

protected:
	
	int m_iWidth;			// width of the bit image (in chars)
	int m_iLength;			// length of the bit image (in chars)
	
	UCHAR *m_BitImage;
	
	float m_fPixelSize;					// Each pixel of the image is a square area
	int m_iImageWidth;			// width of the bit image ( in pixels)
	int m_iImageLength;			// length of the bit image ( in pixels)
	
};

inline void SetUCharBit(UCHAR& uch, int shift, bool bSet=true)
{
	if(bSet)
	{
		uch |= 1<<shift;
	}
	else
	{
		uch &= UCHAR_BITWISE_NOT(1<<shift);
	}
}

inline void CBitImage::InitZero(int iWidth, int iLength, float fPixelSize)
{
	// Firstly, we release myself
	Release();

	m_iWidth=iWidth >> 3;
//	int iLastBitWidth=iWidth & 0x7; 
	if(iWidth & 0x7) m_iWidth++;
	m_iLength = iLength;
	
	int iImageSize = m_iWidth * m_iLength;
	m_BitImage = new UCHAR[iImageSize];
	memset(m_BitImage, 0, iImageSize);
}

inline void CBitImage::InitNoneZero(int iWidth, int iLength, float fPixelSize)
{
	// Firstly, we release myself
	Release();
	
	//*
	m_iWidth=iWidth >> 3;
	if(iWidth & 0x7) m_iWidth++;
	m_iLength = iLength;
	
	int iImageSize = m_iWidth * m_iLength;
	m_BitImage = new UCHAR[iImageSize];
	memset(m_BitImage, 0xff, iImageSize);
	//*/
}

bool CBitImage::Save( FILE *pFileToSave )
{
	if(!pFileToSave) return false;
	
	DWORD dwWrite, dwWriteLen;
	// write the Version info
	dwWrite=BITIMAGE_VER;
	dwWriteLen = fwrite(&dwWrite, 1, sizeof(DWORD), pFileToSave);
	if(dwWriteLen != sizeof(DWORD))
		return false;

	// write the buf size
	DWORD BufSize=4* sizeof(int) + sizeof(float)+m_iWidth * m_iLength;
	dwWrite= BufSize;
	dwWriteLen = fwrite(&dwWrite, 1, sizeof(DWORD), pFileToSave);
	if(dwWriteLen != sizeof(DWORD))
		return false;
	
	// write the following info in order
	// 1. m_iWidth
	// 2. m_iLength
	// 3. m_iImageWidth
	// 4. m_iImageLength
	// 5. m_fPixelSize
	// 6. data in m_BitImage
	UCHAR *buf=new UCHAR[BufSize];
	int cur=0;
	* (int *) (buf+cur) = m_iWidth;
	cur+=sizeof(int);

	* (int *) (buf+cur) = m_iLength;
	cur+=sizeof(int);

	* (int *) (buf+cur) = m_iImageWidth;
	cur+=sizeof(int);

	* (int *) (buf+cur) = m_iImageLength;
	cur+=sizeof(int);

	* (float *) (buf+cur) = m_fPixelSize;
	cur+=sizeof(float);	

	memcpy(buf+cur, m_BitImage, m_iWidth*m_iLength);

	dwWriteLen = fwrite(buf, 1, BufSize, pFileToSave);
	if(dwWriteLen != BufSize)
	{
		delete [] buf;
		return false;
	}
		
	delete [] buf;
	return true;
	
}

bool CBitImage::Load( FILE *pFileToLoad )
{
	if(!pFileToLoad) return false;
	
	DWORD dwRead, dwReadLen;

	// Read the Version
	dwReadLen = fread(&dwRead, 1, sizeof(DWORD), pFileToLoad);
	if(dwReadLen != sizeof(DWORD))
		return false;

	if(dwRead==BITIMAGE_VER)
	{
		// Current Version
		
		// Read the buf size
		dwReadLen = fread(&dwRead, 1, sizeof(DWORD), pFileToLoad);
		if(dwReadLen != sizeof(DWORD))
			return false;
		
		int BufSize=dwRead;
		UCHAR * buf = new UCHAR[BufSize];

		// Read the data
		dwReadLen = fread(buf, 1, BufSize, pFileToLoad);
		if( dwReadLen != (DWORD)BufSize )
		{
			delete [] buf;
			return false;
		}
			
		Release();			// Release the old data

		int cur=0;
		m_iWidth=* (int *) (buf+cur);
		cur+=sizeof(int);
		m_iLength=* (int *) (buf+cur);
		cur+=sizeof(int);
		
		m_iImageWidth=* (int *) (buf+cur);
		cur+=sizeof(int);
		m_iImageLength=* (int *) (buf+cur);
		cur+=sizeof(int);
		m_fPixelSize=* (float *) (buf+cur);
		cur+=sizeof(float);
		
		m_BitImage = new UCHAR[ m_iWidth * m_iLength ];
		memcpy(m_BitImage, buf+cur, m_iWidth * m_iLength);

		delete [] buf;
		return true;

	}
	else
		return false;
}

} // end of namespace

#endif
