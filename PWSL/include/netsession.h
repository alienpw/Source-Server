/*
 *
 *		NetSession�࣬ �����ά����һ�����ӵ�״̬�����ͺͽ��ܵ����ݣ����ܺ�������һЩ����
 *		���ߣ�δ֪
 *		�޸ģ�2009-06 ���� ������  �޸��˺ܶ��߼��ͽṹ
 *		��˾������ʱ��
 *
 */

#include "netio.h"
#include "mutex.h"
#include "security.h"
#include "conf.h"
#include <vector>
#include <stdio.h>

#ifndef __ONLINEGAME_NETSESSION_H__
#define __ONLINEGAME_NETSESSION_H__

namespace GNET
{

class Octets;
class Security;
class NetSession
{
	friend class NetIO;
	friend class StreamIO;
	friend class DgramServerIO;
	friend class DgramClientIO;
	friend class UnixIO;
	enum { DEFAULTIOBUF = 8192 };
protected:
	PollIO  *_assoc_io;			//��session ��������IO����
	bool	_sending;			//�Ƿ������ݵȴ�����
	bool    _closing;			//�Ƿ����ڵȴ��ر�, ���ܻ�ȵ����ͻ���ȫΪ��ʱ�Źر�
	bool    _close_discard;			//�Ƿ���Թر��¼��� ������������ļ����õ� ��;�в�����
						//���Ե������ǵ�Ϊfalseʱ���ر�POLLIO�Ķ������ӳٵ����ͻ���Ϊ���Ժ�
	int     _status;				//����״̬��errno
	size_t	_output_wait_size;		//�ж������ݵȴ����ͣ���PSessionÿ�ε���Send����и��� ����˲�����ȫ׼ȷ�ģ������ӳٷ��͵�һ���ο�ֵ 

	//�±�������������������д���������������ֻ��һ��POLLIO�߳��ڲ��������Կ��Բ�����
	//�����������������Ĵ�ŵ�
	Octets	_ibuffer;			//�����Ӷ���������ԭʼ���ݴ�ŵĻ����� 
	Octets	_obuffer;			//Ҫ���ͳ�ȥ���������ݻ�����
	size_t  _obuffer_offset;		//_obuffer����Ч���ݵ���ʼ, ֮ǰ�����Ѿ�����������  Ŀǰֻ��StreamIOʹ��
	size_t  _ibuffer_maxsize;		//�������϶�ȡʱ, _ibuffer����󳤶ȡ���������ǰ��_ibuffer.capacity()
	size_t  _obuffer_maxsize;
	size_t	_buffer_threshold;		//���ͼ������Config���bufferthreshold��ע��

	//����ֻ�����ڽ��յ����ݺ󣬼���ֻ�����ڽ�Ҫ��������ǰ���Ҷ�ֻ��POLLIO�߳������������Բ���������������
	//�����������������Ĵ�ŵ�
	Octets  _iplainbuf;			//��_ibuffer���ܺ�õ����������ݣ��������ڽ�����
	Octets  _oplainbuf;			//Ҫ���͵�ԭʼ���ݣ��������ܺ�����_obuffer��

	Security *_decrypt;			//�����ʹ�õļ���ģ�飬���ڽ��ܽ��������
	Security *_encrypt;			//���Ͷ�ʹ�õļ���ģ�飬���ڼ��ܷų�������

	std::vector<int> _send_fd_list;		//����fd�Ļ������������fd���ᾡ���ȷ���fd��
	int _cur_send_fd;			//����֧�ַ���FD��session,������浱ǰ���͵�fd

	Octets& GetOBuffer();	//��ȡ������壬�ڻ�ȡ��ͬʱ����ͼ���ܡ�  StreamIO�Ѿ����ٵ������������
	bool UpdateOutBuffer();	//����������壬����м��ܡ����û�мӵ��ܣ��򷵻�false.  ֻ��StreamIOʹ���������, �������������ֵ����һЩ�Ż�

	inline Octets& GetIBuffer() { return _ibuffer; }	//��ȡ���뻺��(��δ���ܵ�)

	inline	bool SendFinish()	//����POLLIO���ͳɹ���. ���ô˺���ʱ����NetSession�������� ����ֵ�����Ƿ�û��������
	{
		if ( NoMoreDataForSend() )
		{
			_sending = false;
			_assoc_io->ForbidSend();
			return true;
		}
		return false;
	}

	void _Close()	//�رա�NetSession�ڲ�ʹ�ã���ֱ��ʹ��
	{
		if ( _closing ) return;
		_closing = true;
		if (_close_discard)	//���ۻ�����������Ϣ������
		{
			_assoc_io->Close();
			return;
		}
		//��ͼ����һ����Ϣ ��û������SIGPIPEʱ����ܻ�Broke pipe
		OnSend();
		if(_obuffer.empty()) UpdateOutBuffer();
		int send_bytes = _obuffer.size() - _obuffer_offset;
		if (send_bytes)
		{
			send_bytes = write(_assoc_io->GetFD(), (char*) _obuffer.begin() + _obuffer_offset, send_bytes);
			if (send_bytes >0)
			{
				_obuffer_offset += send_bytes;
				if ( _obuffer.size() == _obuffer_offset) { _obuffer.clear(); _obuffer_offset = 0;}
			}
		}

		//�رտ��ܴ��ڵĴ�����send_fd_list(������UNIXIO)
		if(!_send_fd_list.empty()) std::for_each(_send_fd_list.begin(), _send_fd_list.end(), ::close);
		_send_fd_list.clear();
		if(_cur_send_fd != -1) close(_cur_send_fd);
		_cur_send_fd = -1;
		//���۷��ͳɹ���񣬶������Ӷϵ�
		_assoc_io->Close();
		return;
	}

protected:
	GNET::Mutex _locker;
	virtual ~NetSession ()
	{
		_decrypt->Destroy();
		_encrypt->Destroy();
	}
	NetSession(const NetSession &rhs) : _assoc_io(rhs._assoc_io), _sending(false), _closing(false), _close_discard(rhs._close_discard),_status(0),
		_obuffer_offset(0),_ibuffer_maxsize(rhs._ibuffer_maxsize),_obuffer_maxsize(rhs._obuffer_maxsize),_buffer_threshold(rhs._buffer_threshold),
		_decrypt(rhs._decrypt->Clone()), _encrypt(rhs._encrypt->Clone()), _locker()
	{
		_output_wait_size = 0;
		_ibuffer.reserve(_ibuffer_maxsize);
		_obuffer.reserve(_obuffer_maxsize);
		_cur_send_fd = -1;
	}

	inline bool Output(const Octets &data)  //��һ������ѹ���Է���
	{
		_oplainbuf.insert(_oplainbuf.end(), data.begin(), data.end());
		return true;
	}

	inline void ClearBufByThreshold(Octets& data)
	{
		if(_buffer_threshold && data.capacity() > _buffer_threshold)
		{
			Octets clear_buf;
			clear_buf.swap(data);
		}
		else
		{
			data.clear();
		}
	}

	Octets& Input();		  //������뻺�壬�Ѿ�����ܵ�

	inline void SendReady()	//���û���os�������������ݺ��ô����������Ͳ���,���ô˺���ʱ����NetSession���봦������״̬
	{
		if ( _sending ) return;
		_sending = true;
		_assoc_io->PermitSend();
	}
public:
	void SetISecurity(Security *s);	//�����������ģ��, �ڲ������,  s��Session���ͷ�
	void SetOSecurity(Security *s);	//�����������ģ��, �ڲ������,  s��Session���ͷ�

	struct Config
	{
		//NetSession����Ҫ�Ĳ���
		size_t ibuffermax;		//���ܻ����С
		size_t obuffermax;		//���ͻ����С
		size_t bufferthreshold;		//�ڲ��������ͻ���������ֵ���������ֵ���ڼ��ܺͷ�����ɺ����������������ڴ潫���ͷţ����ڻ�������Ҫ��������һ��Э�飬���Ի�ռ�õ�Ԥ��֮����ڴ棩
						//���ֵΪ0���ͷſռ� ����ʹ��obuffermanx��2��~3����С��
		Octets decrypt_key;
		Octets encrypt_key;
		unsigned char decrypt_type;	//�յ��Զ���Ϣ��Ľ��ܷ�ʽ
		unsigned char encrypt_type;	//������Ϣ���Զ�ʱʹ�õļ��ܷ�ʽ
		bool close_discard;
		bool latency_mode;		//0 ��poll��ʱ��ŷ������ݣ�������ģʽ��writeϵͳ���õĴ�������     1 ���������̷��ͣ����ӳ�ģʽ ͬʱ���ܺ�ѹ���ᾡ������ʱ���

		//NetIO����Ҫ�Ĳ���, ͨ�������ֻ���޸�port��address
		short socket_type;	//0--tcp, 1--udp, 2--unix 3--unix(support fd transfer)
		unsigned short port;	//���������ӵĶ˿� for tcp & udp
		std::string address;	//clientʱ���ǶԶ˵�IP��ַ������������serverʱ������Ҫ�󶨵ĵ�ַ
		int so_sndbuf;		//Ϊ0ʱ��ʹ��ϵͳĬ�ϵ�ֵ 
		int so_rcvbuf;		//Ϊ0ʱ��ʹ��ϵͳĬ�ϵ�ֵ 
		int nodelay;		//TCP_NODELAY
		int broadcast;
		int listen_backlog;	//server��ʽʱ��listenʱ�����Ĳ���

		Config(): ibuffermax(DEFAULTIOBUF), obuffermax(DEFAULTIOBUF), bufferthreshold(0), decrypt_type(0), encrypt_type(0), close_discard(false),latency_mode(0),
			socket_type(0), port(0), so_sndbuf(0), so_rcvbuf(0), nodelay(1), broadcast(0), listen_backlog(0)
		{
		}

		//��Conf�ļ��м��ء����Ǳ�׼��ģʽ������ǰ��share��ʹ�õĹؼ�������ͬ��
		bool Load(const char *identification, Conf *pconf = NULL)
		{
			Conf *conf = pconf ? pconf: Conf::GetInstance();

			Conf::section_type section = identification;
			Conf::value_type type = conf->find(section, "type");
			if(!strcasecmp(type.c_str(), "tcp")) socket_type = 0; else
			if(!strcasecmp(type.c_str(), "udp")) socket_type = 1; else
			if(!strcasecmp(type.c_str(), "unix")) socket_type = 2; else 
			if(!strcasecmp(type.c_str(), "unixio")) socket_type = 3; else return false;

			port = atoi(conf->find(section, "port").c_str());
			address =  conf->find(section, "address").c_str();
			so_sndbuf = atoi(conf->find(section, "so_sndbuf").c_str());
			so_rcvbuf = atoi(conf->find(section, "so_rcvbuf").c_str());
			nodelay = atoi(conf->find(section, "tcp_nodelay").c_str());
			broadcast = atoi(conf->find(section, "so_broadcast").c_str());
			listen_backlog = atoi(conf->find(section, "listen_backlog").c_str());

			ibuffermax = atoi(conf->find(section, "ibuffermax").c_str());
			obuffermax = atoi(conf->find(section, "obuffermax").c_str());
			bufferthreshold = atoi(conf->find(section, "bufferthreshold").c_str());
			if(int tmp = atoi(conf->find(section, "isec").c_str()))
			{
				Conf::value_type key = conf->find(section, "iseckey");
				decrypt_key = Octets(&key[0], key.size());
				decrypt_type = tmp;
			}

			if(int tmp = atoi(conf->find(section, "osec").c_str()))
			{
				Conf::value_type key = conf->find(section, "oseckey");
				encrypt_key = Octets(&key[0], key.size());
				encrypt_type = tmp;
			}

			close_discard = atoi(conf->find(section, "close_discard").c_str());
			latency_mode = atoi(conf->find(section, "send_no_latency").c_str());
			return true;
		}
	};

	//������õĲ���, ��������Դ��ļ��ж�ȡ,Ȼ������cnf��, Ĭ��ʵ�ַ�ʽ��Ӧ���Ǵ�Manager�п�һ��
	virtual void GetConfig(Config &cnf)const = 0;

	virtual void Init()
	{
		Config cnf;
		GetConfig(cnf);

		if (cnf.ibuffermax) { _ibuffer.reserve(cnf.ibuffermax); _ibuffer_maxsize = cnf.ibuffermax;}
		if (cnf.obuffermax) { _obuffer.reserve(cnf.obuffermax); _obuffer_maxsize = cnf.obuffermax;}
		_close_discard = cnf.close_discard;
		_buffer_threshold = cnf.bufferthreshold;
		if (cnf.decrypt_type)
		{
			Security * sec = Security::Create(cnf.decrypt_type);
			sec->SetParameter(cnf.decrypt_key);
			SetISecurity(sec);
		}
		if (cnf.encrypt_type)
		{
			Security * sec = Security::Create(cnf.encrypt_type);
			sec->SetParameter(cnf.encrypt_key);
			SetOSecurity(sec);
		}
	}

	void SetDelayMode(bool nodelay)
	{
		if(_assoc_io) _assoc_io->SetDelayMode(nodelay);
	}

	NetSession() : _assoc_io(NULL), _sending(false), _closing(false), _close_discard(false),_status(0),
		_obuffer_offset(0), _ibuffer_maxsize(DEFAULTIOBUF),_obuffer_maxsize(DEFAULTIOBUF), _buffer_threshold(0),
		_decrypt(new NullSecurity), _encrypt(new NullSecurity),_locker()
		{ 
			_ibuffer.reserve(_ibuffer_maxsize);
			_obuffer.reserve(_obuffer_maxsize);
		}


	void Close( int code=0, bool locked=false);  //�ر�һ��session. codeΪ�����롣����Ѿ������������ʱlocked�봫��true
	void ErrorClose(int code);		     //�رա������øú���ʱsession���봦������״̬
	int GetStatus() {  return _status; }
private:
	virtual bool NoMoreDataForSend() const = 0;		//������ģ��һ����ʾ���Ƿ���������Ҫ����, ��û�н���رշ��ͣ�����һ��SendReady������ʱ�Żỽ��
	virtual void OnRecv() = 0;				//�յ�������Ϣ���������������Զ�������Ϣ���н��롢����������һ���Դ����ꡣ�������������Ϣδ������
								//ֻ�е���һ����������Ϣ�¼������л����ٴδ�����
	virtual void OnRecvFD(int fd) = 0;			//����֧�ִ���fd��UnixIO�����յ�fdʱ��������������
	virtual void OnSend() = 0;				//socket�Ѿ���д��׼�����ͣ���ʱӦ�õ���Output������Ҫ���͵�����ѹ��
	virtual void OnUnixSend() = 0;				//֧��UnixIO��������fd������ĺ�����ֻ��UnixIO�����������������������OnSend
	virtual void OnOpen(const SockAddr& local, const SockAddr& peer) { }
	virtual void OnOpen() { }
	virtual void OnClose() { }
public:
	virtual std::string Identification () const = 0;	//��session�ı�ʶ, Ϊ���ּ����ԣ�Ŀǰ��ʵ����Manager��Identificationһ��
	virtual int IntIdentification() const {return -1;}	//ʹ��Int�ı�ʶ,�������ڵ������
	virtual void OnAbort(const SockAddr& sa) { }		//����ʧ��,���ͻ������������ʧ��ʱ�Ĵ���
	virtual	NetSession* Clone() const = 0;
	virtual void Destroy() { delete this; }
	virtual void OnCheckAddress(SockAddr &) const { }
protected:
	int Detach(Octets & o_plainbuf, Octets & o_outputbuf, Octets & o_securebuf, Octets & i_securebuf);	//�ڲ�ʹ��
	int Attach(const Octets & o_plainbuf, const Octets & o_outputbuf, const Octets & o_securebuf, const Octets & i_securebuf);	//�ڲ�ʹ��

};

}

#endif
