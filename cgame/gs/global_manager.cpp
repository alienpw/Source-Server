#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ASSERT.h>
#include <threadpool.h>
#include <conf.h>
#include <io/pollio.h>
#include <io/passiveio.h>
#include <gsp_if.h>
#include <db_if.h>
#include <amemory.h>
#include <meminfo.h>
#include <glog.h>

#include "template/itemdataman.h"
#include "global_manager.h"
#include "player_imp.h"
#include "playertemplate.h"
#include "template/globaldataman.h"
#include "task/taskman.h"
#include "aei_filter.h"

bool 
global_world_manager::InitNetClient(const char * gmconf)
{
	extern unsigned long _task_templ_cur_version;
	char version[1024];
	int ele_version = ELEMENTDATA_VERSION;
	int task_version = _task_templ_cur_version;
	int gshop_version = globaldata_getmalltimestamp();
	int gdividendshop_version = globaldata_getmall2timestamp();
	sprintf(version, "%x%x%x%x", ele_version, task_version, gshop_version, gdividendshop_version);


	rect rt = _plane.GetLocalWorld();
	ASSERT(rt.left < rt.right && rt.top < rt.bottom);
	GMSV::InitGSP(gmconf,world_manager::GetWorldIndex(),_world_tag,rt.left,rt.right,rt.top,rt.bottom, version);
	GDB::init_gamedb();
	return true;
}

static bool quit_flag = false; 
static void timer_thread()
{
	g_timer.timer_thread();
}

static void poll_thread()
{
	for(;!quit_flag;)
	{
		ONET::PollIO::Poll(50);
	}
}
static void str2rect(rect & rt,const char * str)
{
	sscanf(str,"{%f,%f} , {%f,%f}",&rt.left,&rt.top,&rt.right,&rt.bottom);
}

static void timer_tick(int index,void *object,int remain)
{
	class World_Tick_Task : public ONET::Thread::Runnable , public abase::ASmallObject
	{
		public:
			world_manager * _man;
			World_Tick_Task(world_manager * man):_man(man){}
			virtual void Run()
			{
				_man->Heartbeat();
				delete this;
			}
	};
	
	world_manager * pMan = (world_manager *)object;
	ONET::Thread::Pool::AddTask(new World_Tick_Task(pMan));
}

int
global_world_manager::Init(const char * gmconf_file,const char *  servername)
{
	_message_handler = CreateMessageHandler();
	_manager_instance = this;

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	sigprocmask(SIG_BLOCK, &set, NULL);

	/*
	 *      ��ʼ��ʼ������
	 */
	Conf *conf = Conf::GetInstance();
	Conf::section_type section = "World_";
	section += servername;

	//�������ж�ȡNPC,PLAYER����Ʒ����
	_player_max_count = atoi(conf->find(section,"player_capacity").c_str());
	_npc_max_count = atoi(conf->find(section,"npc_count").c_str());
	_matter_max_count = atoi(conf->find(section,"matter_count").c_str());
	if(_player_max_count > GL_MAX_PLAYER_COUNT || _player_max_count <= 0) _player_max_count=GL_MAX_PLAYER_COUNT;
	if(_npc_max_count > GL_MAX_NPC_COUNT || _npc_max_count <= 0) _npc_max_count = GL_MAX_NPC_COUNT;
	if(_matter_max_count > GL_MAX_MATTER_COUNT || _matter_max_count <=0) _matter_max_count = GL_MAX_MATTER_COUNT;

	//��ʼ�����еĹ�����
	world_manager::Init();

	if(int irst = world_manager::InitBase(section.c_str()))
	{
		//��ʼ����������
		return irst;
	}

	//�õ���Ŀ¼
	std::string root = conf->find("Template","Root");

	//�õ�����Ŀ¼
	std::string base_path;
	base_path = root + conf->find(section,"base_path");
	__PRINTINFO("��Դ��Ŀ¼:'%s'\n", base_path.c_str());

	//�õ����������Ĳ���
	_restart_shell  = base_path + conf->find("Template","RestartShell");

	//�õ�������õ�cid
	if(!_cid.Init(conf->find(section,"cid").c_str()))
	{
		__PRINTF("�����classid �� 'cid'��\n");
		return -1008;
	}


	_plane.Init(_world_index);
	_plane.InitManager(this);
	_plane.w_activestate = 1;	//��������ʼ�ռ����

	/*
	 *      ��ʼ����������
	 *
	 */
	std::string str = conf->find(section,"grid");
	int row=800,column=800;
	float xstart=0.f,ystart=0.f,step=12.5f;
	sscanf(str.c_str(),"{%d,%d,%f,%f,%f}",&column,&row,&step,&xstart,&ystart);

	if(!_plane.CreateGrid(row,column,step,xstart,ystart)){
		__PRINTF("Can not create world!\n");
		return -1;
	}

	rect rt = _plane.GetGrid().grid_region;
	__PRINTF("Create grid: %d*%d with step %f\n",row,column,step);
	__PRINTF("Grid Region: {%.2f,%.2f} - {%.2f,%.2f}\n",rt.left,rt.top,rt.right,rt.bottom);
	
	rect local_rt,base_rt;
	str2rect(base_rt,conf->find(section,"base_region").c_str());
	str2rect(local_rt,conf->find(section,"local_region").c_str());
	if(!_plane.GetGrid().SetRegion(local_rt,base_rt,GRID_SIGHT_RANGE+30.f))
	{
		__PRINTF("�����ļ��е��������ݲ���ȷ(base_region/local_region)\n");
		return -2;
	}

	str2rect(rt,conf->find(section,"inner_region").c_str());
	_plane.GetGrid().inner_region = rt;
	float grid_sight_range = atof(conf->find(section, "grid_sight_range").c_str()); 
	if(grid_sight_range < 20.f || grid_sight_range > 100.f) grid_sight_range = GRID_SIGHT_RANGE;
	_plane.BuildSliceMask((grid_sight_range>30.f ? 30.f : grid_sight_range), grid_sight_range); 

	rt = _plane.GetGrid().local_region;
	__PRINTF("Local Region: {%.2f,%.2f} - {%.2f,%.2f}\n",rt.left,rt.top,rt.right,rt.bottom);
	rt = _plane.GetGrid().inner_region;
	__PRINTF("Inner Region: {%.2f,%.2f} - {%.2f,%.2f}\n",rt.left,rt.top,rt.right,rt.bottom);
	if(rt.left > rt.right - GRID_SIGHT_RANGE || rt.top > rt.bottom - GRID_SIGHT_RANGE)
	{
		__PRINTF("�ڲ������С\n");
		return -5;
	}

	//��ʼ����ͼ��Դ: ���Ρ�Ѱ·����ײ������ͼ
	if(_mapres.Init(servername, base_path, _plane.GetLocalWorld(), &_plane) != 0)
	{
		__PRINTF("��ʼ����ͼ��Դʧ��\n");
		return -6;	
	}
	if(_mapres.GetType() != MAPRES_TYPE_ORIGIN)
	{
		__PRINTF("global world��֧�ֶ�̬��ͼ��Դ\n");
		return -6;
	}

	std::string  regionfile = base_path + conf->find("Template","RegionFile");
	std::string  regionfile2 = base_path + conf->find("Template","RegionFile2");
	std::string pathfile = base_path + conf->find("Template","PathFile");

	//����player_temp���޸������ļ�������Ҫ�ȱ���һ���ļ�����
	if(!player_template::Load("ptemplate.conf",&_dataman))
	{
		__PRINTF("can not load player template data from file template file or 'ptemplate.conf'\n");
		return -7;
	}

	//װ�س�������
	if(!player_template::LoadRegionData(regionfile.c_str(),regionfile2.c_str()))
	{
		__PRINTF("can not load city region data from file '%s'\n",regionfile.c_str());
		return -7;
	}

	player_template::GetRegionTime(_region_file_tag,_precinct_file_tag);
    //��ʼ��ÿ�������ڵĴ��͵� 
    world_manager::InitRegionWayPointMap();
	//��ʼ��slice��region����״��	
	city_region::InitSliceOverlapRegion(_plane.GetGrid());	
	//��ȡ·���ļ�
	if(!_pathman.Init(pathfile.c_str()))
	{
		__PRINTF("�޷���·���ļ�\n");
		return -9;
	}

	//��ʼ��NPC������
	if(!_mapres.BuildNpcGenerator(&_plane))
	{
		__PRINTF("�޷�����npc������\n");
		return -8;
	}
	
	//��ʼ����������
	if(!InitPublicQuestSystem())
	{
		__PRINTF("��ʼ����������ʧ��\n");
		return -10;	
	}

	//��ʼ������ϵͳ
	if(atoi(conf->find(section,"wedding_server").c_str()) == 1)
	{
		_wedding_man = new wedding_manager;
		if(!_wedding_man || !_wedding_man->Initialize())
		{
			__PRINTF("��ʼ������ϵͳʧ��. world_tag=%d\n", _world_tag);
			return -11;	
		}
		__PRINTF("wedding server ����. world_tag=%d\n", _world_tag);
	}
	
	//��ʼ��dps���а�ϵͳ
	if(atoi(conf->find(section,"dps_rank").c_str()) == 1)
	{
		_dps_rank_man = new dps_rank_manager;	
		if(!_dps_rank_man || !_dps_rank_man->Initialize())
		{
			__PRINTF("��ʼ��dps���а�ϵͳʧ��. world_tag=%d\n",_world_tag);
			return -11;
		}
		__PRINTF("dps rank ����. world_tag=%d\n", _world_tag);
	}
	
	/*
	 *       ������ʱ���߳� 
	 */
	ONET::Thread::Pool::CreateThread(timer_thread);

	//��ʼ����������tick
	g_timer.set_timer(1,0,0,timer_tick,this);

	/**
	 *      ��ʼ��PollIO��������Ӧ�� Poll�߳�
	 */
	ONET::PollIO::Init();
	ONET::Thread::Pool::CreateThread(poll_thread);

	/*
	 *      ��ʼ����Ϸ������֮������ӳ�
	 */
	if(!InitNetIO(servername))
	{
		return -7;
	}

	/**
	 *      ��ʼ��������Ϣ����  ע����Ϣ���е�OnTimer���������
	 */
	_msg_queue.Init(&_plane,g_timer);

	InitNetClient(gmconf_file);
	GLog::init();	//����ĳ�ʼ��������io ���ʼ��֮��
	// �����־�����ܼ�¼�ϵģ������˰�
	//GLog::log(GLOG_INFO,"gameserver %d start", _world_index);
	
	//�ͷ����е�С͹������
	trace_manager2::ReleaseElement();        
	
	//��󴴽������ݿ��ȡserverdata��task
	//Ŀǰֻ�л���ϵͳ��dps���а�ʹ��serverdata
	if(IsWeddingServer() || HasDpsRank())
	{
		_load_task = new serverdata_load_task(this);
		if(!_load_task)
		{
			__PRINTF("���������ݿ��ȡserverdata��taskʧ��. world_tag=%d\n", _world_tag);
			return -12;
		}
		__PRINTF("��ʼ����ȡserverdata��task. world_tag=%d load_task=%p timer_index=%d\n", 
				_world_tag, _load_task, _load_task->GetTimerIndex());
	}	
	return 0;
}

void 
global_world_manager::Heartbeat()
{
	_msg_queue.OnTimer(0,100);
	world_manager::Heartbeat();
	_plane.RunTick();
	PublicQuestRunTick();
	if(_wedding_man) 
		_wedding_man->RunTick();
	if(_dps_rank_man)
		_dps_rank_man->RunTick();
}

bool 
global_world_manager::InitNetIO(const char * servername)
{
	_ioman.SetPlane(this);
	grid & g = _plane.GetGrid();
	return _ioman.Init(servername,g.local_region,g.inner_region);
}

void 
global_world_manager::GetPlayerCid(player_cid & cid)
{
	cid = _cid;
}

void 
global_world_manager::SendRemoteMessage(int id, const MSG & msg)
{
	return _ioman.SendMessage(id,msg);
}

int  
global_world_manager::BroadcastSvrMessage(const rect & rt,const MSG & message,float extend_size)
{
	return _ioman.BroadcastMessage(rt,message,extend_size);
}

void 
global_world_manager::PostMessage(world * plane, const MSG & msg)
{
	ASSERT(plane == &_plane);
	_msg_queue.AddMsg(msg);
}

void 
global_world_manager::PostMessage(world * plane, const MSG & msg,int latancy)
{
	ASSERT(plane == &_plane);
	_msg_queue.AddMsg(msg,latancy);
}

void 
global_world_manager::PostMessage(world * plane, const XID * first, const XID * last, const MSG & msg)
{
	ASSERT(plane == &_plane);
	_msg_queue.AddMultiMsg(first,last,msg);
}

void 
global_world_manager::PostPlayerMessage(world * plane, int * player_list, size_t count, const MSG & msg)
{
	ASSERT(plane == &_plane);
	_msg_queue.AddPlayerMultiMsg(count, player_list,msg);
}

void
global_world_manager::PostMultiMessage(world * plane,abase::vector<gobject*,abase::fast_alloc<> > &list, const MSG & msg)
{
	ASSERT(plane == &_plane);
	_msg_queue.AddMultiMsg(list, msg);
}


int 
global_world_manager::GetServerNear(const A3DVECTOR & pos) const
{
	return _ioman.GetServerNear(pos);
}

int 
global_world_manager::GetServerGlobal(const A3DVECTOR & pos) const
{
	return _ioman.GetGlobalServer(pos,_world_tag);
}


void 
global_world_manager::RestartProcess()
{
	//�����������˶����� 
	gplayer * pPool = GetPlayerPool();
	for(size_t i = 0; i<world_manager::GetMaxPlayerCount(); i ++)
	{
		if(pPool[i].IsEmpty()) continue;
		if(!pPool[i].imp) continue;
		int cs_index = pPool[i].cs_index;
		if(cs_index <=0) continue;
		GMSV::SendDisconnect(cs_index,pPool[i].ID.id,pPool[i].cs_sid,0);
	}
	if(!fork())
	{
		for(int i =3;i < getdtablesize(); i ++)
		{
			close(i);
		}
		sleep(1);
		system(_restart_shell.c_str());
	}
}

gplayer* 
global_world_manager::FindPlayer(int uid, int & world_index)
{
	int index = _plane.FindPlayer(uid);
	if(index < 0) return NULL;
	world_index = 0;
	return _plane.GetPlayerByIndex(index);
}

int
global_world_manager::SendRemotePlayerMsg(int uid, const MSG & msg)
{
	int w_idx;
	if((w_idx = GetPlayerServerIdx(uid)) >= 0)
	{
		if(w_idx != _world_index)
		{
			SendRemoteMessage(w_idx,msg);
		}
		else
		{
			//�����Լ�
			if(msg.ttl > 0) _msg_queue.AddMsg(msg);
		}
	}
	else
	{
		__PRINTF("can not find extern player %d(%d)\n",uid,w_idx);
	}
	return w_idx;
}
	
world * 
global_world_manager::GetWorldByIndex(size_t index)
{
	ASSERT(index == 0);
	return &_plane;
}

size_t
global_world_manager::GetWorldCapacity()
{
	return 1;
}

void 
global_world_manager::HandleSwitchRequest(int link_id,int user_id, int localsid,int source, const instance_key & key)
{
	world * pPlane = &_plane;
	int index1 = pPlane->FindPlayer(user_id);
	if(index1 >= 0) 
	{
		GLog::log(GLOG_WARNING,"���ܵ��л�����ʱ�û�%d�Ѵ���(%d)",user_id,GetPlayerPool()[index1].login_state);
		//����û���Ӧ�ô��ڵ�
		return;
	}
	gplayer * pPlayer = pPlane->AllocPlayer();
	if(pPlayer == NULL)
	{
		//����û�������ռ��������Player����Ϣ
		GLog::log(GLOG_WARNING,"�û�%d��ת�Ʒ�����ʱ�ﵽ�������ֵ",user_id);
		//Ŀǰ�������κ��޷�ת�Ƶ���Ϣ�����޻�����Ϣ
		return;
	}
	__PRINTF("player %d switch from %d\n",user_id,source );
	pPlayer->cs_sid = localsid;
	pPlayer->cs_index = link_id;
	pPlayer->ID.id = user_id;
	pPlayer->ID.type = GM_TYPE_PLAYER;
	pPlayer->login_state = gplayer::WAITING_SWITCH;
	pPlayer->pPiece = NULL;
	if(!pPlane->MapPlayer(user_id,pPlane->GetPlayerIndex(pPlayer)))
	{	
		GLog::log(GLOG_WARNING,"������ת��ʱmap playerʧ��(%d)",user_id);
		//Ŀǰ�������κ��޷�ת�Ƶ���Ϣ�����޻�����Ϣ
		pPlane->FreePlayer(pPlayer);
		pPlayer->Unlock();
		return;
	}
	
	class switch_task : public ONET::Thread::Runnable, public abase::timer_task , public abase::ASmallObject
	{
		gplayer *_player;
		int _userid;
		world * _plane;
		public:
			switch_task(gplayer * pPlayer,world * pPlane):_player(pPlayer),_userid(pPlayer->ID.id),_plane(pPlane)
			{
				//�����ʱʱ����2.5s�ӳ���5s����ֹgs���ظ�����ĳ�ʱ��modify by liuguichen 20131224
				SetTimer(g_timer,TICK_PER_SEC*5,1);
				__PRINTF("timer %p %d\n",this,_timer_index);
			}
		public:
			virtual void OnTimer(int index,int rtimes)
			{
				ONET::Thread::Pool::AddTask(this);
			}

			virtual void Run()
			{
				spin_autolock keeper(_player->spinlock);
				if(_player->IsActived() && _player->ID.id == _userid && _player->login_state == gplayer::WAITING_SWITCH)
				{
					_plane->UnmapPlayer(_userid);
					_plane->FreePlayer(_player);
				}
				delete this;
			}
	};
	//�����ȴ���Ϣ
	MSG msg;
	BuildMessage(msg,GM_MSG_SWITCH_GET,pPlayer->ID,XID(GM_TYPE_SERVER,world_manager::GetWorldIndex()),A3DVECTOR(0,0,0),GetWorldTag(), &key, sizeof(key));
	pPlane->SendRemoteMessage(source,msg);

	//���ó�ʱ
	switch_task *pTask = new switch_task(pPlayer,pPlane);
	pPlayer->base_info.race = (int)(abase::timer_task*)pTask;
	pPlayer->base_info.faction = pTask->GetTimerIndex();
	pPlayer->Unlock();
}

int
global_world_manager::GetOnlineUserNumber() 
{ 
	return _plane.GetPlayerCount();
}

void 
global_world_manager::GetLogoutPos(gplayer_imp * pImp, int & world_tag,A3DVECTOR & pos)
{
	if(_world_limit.savepoint && _save_point.tag > 0)
	{
		world_tag = _save_point.tag;
		pos = _save_point.pos;
	}
	else
	{
		pos = pImp->GetLogoutPos(world_tag);
	}
}

void 
global_world_manager::SwitchServerCancel(int link_id,int user_id, int localsid)
{
//	ASSERT(source == world_manager::GetWorldIndex());
	timeval tv;
	gettimeofday(&tv,NULL);
	__PRINTF("%d�л���������ȡ��:%u.%u\n",user_id,tv.tv_sec,tv.tv_usec);
	int index1;
	gplayer * pPlayer = FindPlayer(user_id,index1);
	if(!pPlayer)
	{
		ASSERT(false);
		//û���ҵ� ���ʵ��û�
		//��������£�����û�Ӧ�ô��ڵ�
		return;
	}
	spin_autolock keeper(pPlayer->spinlock);

	if(pPlayer->ID.id != user_id || !pPlayer->IsActived() || !pPlayer->imp)
	{
		ASSERT(false);
		return;
	}
	pPlayer->imp->CancelSwitch();
}

void	global_user_login(int cs_index,int cs_sid,int uid,const void * auth_data, size_t auth_size, bool isshielduser, char flag);
void 
global_world_manager::UserLogin(int cs_index,int cs_sid,int uid,const void * auth_data, size_t auth_size, bool isshielduser, char flag)
{
	global_user_login(cs_index,cs_sid,uid,auth_data,auth_size,isshielduser,flag);
}

void 
global_world_manager::SetFilterWhenLogin(gplayer_imp * pImp, instance_key * )
{
	if(GetKickoutPoint().tag > 0)
		pImp->_filters.AddFilter(new aegw_filter(pImp,FILTER_CHECK_KICKOUT));
}

bool 
global_world_manager::IsUniqueWorld()
{
	return true;
}

world_message_handler * 
global_world_manager::CreateMessageHandler()
{
	return new global_world_message_handler(this,&_plane);
}

bool
global_world_manager::TriggerSpawn(int sid)
{
	return _plane.TriggerSpawn(sid);
}


bool
global_world_manager::ClearSpawn(int sid)
{
	return _plane.ClearSpawn(sid);
}

bool global_world_manager::WeddingCheckOngoing(int groom, int bride, int scene)
{
	return _wedding_man && _wedding_man->CheckOngoing(groom,bride,scene);
}

bool global_world_manager::WeddingCheckOngoing(int id)
{
	return _wedding_man && _wedding_man->CheckOngoing(id);
}

bool global_world_manager::WeddingSendBookingList(int id, int cs_index, int cs_sid)
{
	return _wedding_man && _wedding_man->SendBookingList(id,cs_index,cs_sid);
}

bool global_world_manager::WeddingCheckBook(int start_time, int end_time, int scene, int card_year, int card_month, int card_day)
{
	return _wedding_man && _wedding_man->CheckBook(start_time, end_time, scene, card_year, card_month, card_day);
}

bool global_world_manager::WeddingTryBook(int start_time, int end_time, int groom, int bride, int scene)
{
	return _wedding_man && _wedding_man->TryBook(start_time, end_time, groom, bride, scene);
}

bool global_world_manager::WeddingCheckCancelBook(int start_time, int end_time, int groom, int bride, int scene)
{
	return _wedding_man && _wedding_man->CheckCancelBook(start_time,end_time,groom,bride,scene);
}

bool global_world_manager::WeddingTryCancelBook(int start_time, int end_time, int groom, int bride, int scene)
{
	return _wedding_man && _wedding_man->TryCancelBook(start_time,end_time,groom,bride,scene);
}

bool global_world_manager::WeddingDBLoad(archive & ar)
{
	return _wedding_man && _wedding_man->DBLoad(ar);
}

bool global_world_manager::WeddingDBSave(archive & ar)
{
	return _wedding_man && _wedding_man->DBSave(ar);
}

bool global_world_manager::DpsRankUpdateRankInfo(int roleid, int level, int cls, int dps, int dph)
{
	return _dps_rank_man && _dps_rank_man->UpdateRankInfo(roleid,level,cls,dps,dph);
}

bool global_world_manager::DpsRankSendRank(int link_id, int roleid, int link_sid, unsigned char rank_mask)
{
	return _dps_rank_man && _dps_rank_man->SendRank(link_id,roleid,link_sid,rank_mask);
}

bool global_world_manager::DpsRankDBLoad(archive & ar)
{
	return _dps_rank_man && _dps_rank_man->DBLoad(ar);
}

bool global_world_manager::DpsRankDBSave(archive & ar)
{
	return _dps_rank_man && _dps_rank_man->DBSave(ar);
}

void global_world_manager::OnDeliveryConnected()
{
	if(GetWorldTag() == 1) 
	{
		_autoteam_man.SendConfigData();
		GMSV::SendMafiaPvPRegister(GetWorldIndex(),GetWorldTag());
	}
}

void global_world_manager::OnMafiaPvPStatusNotice(int status,std::vector<int>& ctrl_list)
{
	if(status)
	{
		world_flags& flags = GetWorldFlag();
		flags.mafia_pvp_flag = true;
		flags.nonpenalty_pvp_flag = true;
	}
	else
	{
		world_flags& flags = GetWorldFlag();
		flags.mafia_pvp_flag = false;
		flags.nonpenalty_pvp_flag = false;
	}
	
	MSG	msg;
	XID wid(GM_TYPE_SERVER,world_manager::GetWorldIndex());
	BuildMessage(msg,GM_MSG_MAFIA_PVP_STATUS,wid,wid,A3DVECTOR(0,0,0),status,&ctrl_list.front(),ctrl_list.size()*sizeof(int));
	PostMessage(&_plane,msg);
}

void global_world_manager::OnMafiaPvPElementRequest(unsigned int version)
{
	MSG	msg;
	XID wid(GM_TYPE_SERVER,world_manager::GetWorldIndex());
	BuildMessage(msg,GM_MSG_MAFIA_PVP_ELEMENT,wid,wid,A3DVECTOR(0,0,0),(int)version);
	PostMessage(&_plane,msg);
}

world_message_handler * countryterritory_world_manager::CreateMessageHandler()
{
	return new countryterritory_world_message_handler(this, &_plane);
}

void countryterritory_world_manager::OnDeliveryConnected()
{
	GMSV::SendCountryBattleServerRegister(0, GetWorldIndex(),GetWorldTag(),-1);
}

void countryterritory_world_manager::NotifyCountryBattleConfig(GMSV::CBConfig * config)
{
	_capital_list.clear();
	for(size_t i=0; i<config->capital_count; i++)
	{
		GMSV::CBConfig::CountryCapital & capital = config->capital_list[i];
		SetCapital(capital.country_id, A3DVECTOR(capital.posx,capital.posy,capital.posz), capital.worldtag);
	}
}

void countryterritory_world_manager::SetFilterWhenLogin(gplayer_imp * pImp, instance_key * )
{
	pImp->_filters.AddFilter(new aect_filter(pImp,FILTER_CHECK_KICKOUT));
	
	int country_id = pImp->GetCountryId();
	if(country_id && IsCapitalPos(country_id,pImp->_parent->pos))
	{
		//���׶�������ΪGM����
		object_interface obj_if(pImp);
		obj_if.SetGMInvisibleFilter(true, -1, filter::FILTER_MASK_NOSAVE);
	}
}

void countryterritory_world_manager::PlayerAfterSwitch(gplayer_imp * pImp)
{
	countryterritory_switch_data * pData = substance::DynamicCast<countryterritory_switch_data>(pImp->_switch_additional_data);
	if(pData)
	{
		pImp->CountryJoinStep2();
	}
	else
	{
		pImp->ClearSwitchAdditionalData();
	}
}

void countryterritory_world_manager::GetLogoutPos(gplayer_imp * pImp, int &world_tag, A3DVECTOR & pos)
{
	int country_id = pImp->GetCountryId();
	if(country_id)
	{
		//�ǳ�������Ϊ�����׶�
		if(GetCapital(country_id, pos, world_tag)) return;
		world_tag = 143;
		pos = A3DVECTOR(0,0,0);
		GLog::log(GLOG_ERR,"�׶���Ϣ������worldtag=%d roleid=%d country=%d", GetWorldTag(), pImp->_parent->ID.id, country_id);
		return;
	}
	pImp->GetCountryKickoutPos(world_tag, pos);
}

bool countryterritory_world_manager::GetTownPosition(gplayer_imp *pImp, const A3DVECTOR &opos, A3DVECTOR &pos, int & tag)
{
	//�سǵ�������ԭ��
	pos = opos;
	tag = _world_tag;
	return true;
}
