/*
	�����ṩ�˲���һ��֤��ĺ�����
	��������һ��֤�飬��һ��֤�飬ȡ��֤����������ݵȵȡ�
*/

#ifndef __GS_CERT_FILE_H__
#define __GS_CERT_FILE_H__

#include <time.h>


class certXImp;
class cert
{
public:
	cert();	//��ɹ����Ժ���һ���հ�֤��
	~cert();
	
public:
	int load(const char * __buf, int __size);
	int save(char * __buf, int size);

public:
	int setStart(time_t __start);
	int setEnd(time_t __end);
	int genKey();
	int setKey(const char *__key);
	int setAuthor(const char * __key, const char * __value);
	int setAcceptor(const char *__key, const char * __value);

enum { KEY_LENGTH = 16};
	class enumcb
	{
		public:
		virtual int operator()(const char * __key, const char *__value) = 0;
	};
	
	time_t getStart();
	time_t getEnd();
	const char * getKey();
	const char * getAuthor(const char *__key);
	const char * getAcceptor(const char *__key);
	int enumAuthor(enumcb & cb);
	int enumAcceptor(enumcb & cb);
private:
	certXImp * _imp;
};

typedef EVP_PKEY EVP_PKEY;
class certfile 
{
public:
	enum { MAX_SIGNATURE_SIZE = 1024};
	enum { MAX_SIZE = 4096};
	certfile(cert *ct):_cert(ct)
	{
		_pubkey = NULL;
		_prikey = NULL;
		_sigbuf = NULL;
		_sigsize = 0;
		_datbuf = NULL;
		_datsize = 0;
	}
	~certfile();
	
public:
	void setPubKey(EVP_PKEY *pubkey){_pubkey = pubkey;}
	void setPriKey(EVP_PKEY *prikey){_prikey = prikey;} //this class  don't free the keys
	
	int loadFile(const char * file);
	int saveFile(const char * file);
	int load(const char * data, int size);	// load base64 data to cert object

	cert * getCert(){return _cert;}


	int refreshData(); 	//��cert������ݱ��data, ���������������֤���ļ��ʹ��̵�ʱ�����Ҫ����
	int storeData();	//��data������ݱ��浽cert������, �������Ǹ������ķ�����

	int sign();		//MD5 sign 	need set private key 
	int verify();		//		need set public key 
	int verify(const char * data, int size);//��base64�����ж�ȡ���ݺ�ǩ��������֤,ͬ����Ҫpublic key

	inline char* getSignature(int &size);
	inline int setSignature(const char * buf, int size);
	inline char* getData(int &size);
	inline int setData(const char * buf , int size);

private:
	cert *_cert;
	EVP_PKEY * _pubkey;
	EVP_PKEY * _prikey;

	char *_datbuf;
	int   _datsize;
	char *_sigbuf;
	int   _sigsize;
};

char * certfile::getSignature(int &size){
	size = _sigsize; 
	return _sigbuf;
}

int certfile::setSignature(const char * buf, int size){
	if(!buf) return -1;
	if(_sigbuf) delete _sigbuf;
	_sigbuf = new char[size];
	memcpy(_sigbuf,buf,size);
	_sigsize = size;
	return 0;
}

char* certfile::getData(int &size){
		size = _datsize; 
		return _datbuf;
}

int certfile::setData(const char * buf , int size){
	if(!buf) return -1;
	if(_datbuf) delete _datbuf;
	_datbuf = new char[size];
	memcpy(_datbuf,buf,size);
	_datsize = size;
	return 0;
}
#endif
