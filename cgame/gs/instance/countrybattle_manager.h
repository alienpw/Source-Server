#ifndef __ONLINEGAME_GS_COUNTRYBATTLE_MANAGER_H__
#define __ONLINEGAME_GS_COUNTRYBATTLE_MANAGER_H__

#include "instance_manager.h"

struct country_battle_param
{
	int battle_id;
	int attacker;		//����������
	int defender;		//���ط�����
	int player_count;	//ÿ��������Ƶ�����
	int end_timestamp;	//����ʱ��
	int attacker_total;	//����������������
	int defender_total;	//���ط�����������
	int max_total;		//�ĸ����������������
};

/*------------------------��սս����������-------------------------------*/
class countrybattle_world_manager : public instance_world_manager 
{
public:
	enum
	{
		BATTLE_TYPE_FLAG = 0,
		BATTLE_TYPE_TOWER = 1,
		BATTLE_TYPE_STRONGHOLD = 2,
	};
	struct tower
	{
		int controller_id;
		int npc_tid;
		int group;
	};
	typedef abase::vector<tower> TOWER_LIST;
	enum
	{
		STRONGHOLD_STATE_ATTACKER = 0,
		STRONGHOLD_STATE_ATTACKER_HALF,
		STRONGHOLD_STATE_NEUTRAL,
		STRONGHOLD_STATE_DEFENDER_HALF,
		STRONGHOLD_STATE_DEFENDER,

		STRONGHOLD_STATE_COUNT,
	};
	struct stronghold
	{
		struct
		{
			int controller_id;
			int mine_tid;
		}data[STRONGHOLD_STATE_COUNT];	//�ݵ�������״̬�¶�Ӧ�Ŀ� [����ռ��,��������ռ��,����,�ط�����ռ��,�ط�ռ��]
		A3DVECTOR pos;
		float squared_radius;
	};
	typedef abase::vector<stronghold> STRONGHOLD_LIST;
private:
	//�׶����ڵ�ͼ����������������ߴ���ʹ��,��deliveryͨ��Э���ʼ��
	struct capital_entry
	{
		int country_id;
		int world_tag;
		A3DVECTOR pos;
	};
	abase::vector<capital_entry> _capital_list;
	bool GetCapital(int country_id, A3DVECTOR &pos, int & tag)
	{
		int list[4];
		int counter = 0;
		for(size_t i=0; i<_capital_list.size() && counter < 4; i++)	
		{
			if(country_id == _capital_list[i].country_id)
			{
				list[counter++] = i;
			}
		}
		if(counter > 0)
		{
			int index = abase::Rand(0,counter-1);
			pos = _capital_list[list[index]].pos;
			tag = _capital_list[list[index]].world_tag;
			return true;
		}
		return false;
	}
	void SetCapital(int country_id, const A3DVECTOR &pos, int tag)
	{
		capital_entry ent;
		ent.country_id = country_id;
		ent.world_tag = tag;
		ent.pos = pos;
		_capital_list.push_back(ent);	
	}
	
	int _battle_type;
	
	//����ģʽ��Ҫ������
	abase::vector<int> _flag_controller_list;
	int _flag_mine_tid;
	bool IsFlagMine(int id){ return id == _flag_mine_tid; }
	
	struct flag_goal
	{
		A3DVECTOR pos;
		float squared_radius;
	};
	flag_goal _attacker_flag_goal; 
	flag_goal _defender_flag_goal; 

	//�ݻٷ�����ģʽ��Ҫ������
	TOWER_LIST _attacker_tower_list;
	TOWER_LIST _defender_tower_list;
	std::set<int>	_total_tower_set;		//tower tid
	bool IsTowerNpc(int tid){ return _total_tower_set.find(tid) != _total_tower_set.end();}
	
	//�ݵ�ģʽ
	STRONGHOLD_LIST _stronghold_list;
	abase::hash_map<int, int> _total_stronghold_map;	//mine tid -> stronghold state
	bool IsStrongholdMine(int tid){ return _total_stronghold_map.find(tid) != _total_stronghold_map.end(); }
	
	struct town_entry
	{
		int faction;
		A3DVECTOR target_pos;
	};
	
	abase::vector<town_entry> _town_list;
	bool GetTown(int faction, A3DVECTOR &pos, int & tag)
	{
		int list[64];
		int counter = 0;
		for(size_t i = 0; i < _town_list.size() && counter < 64; i ++)
		{
			if(_town_list[i].faction & faction)
			{
				list[counter] = i;
				counter ++;
			}
		}
		if(counter > 0)
		{
			int index = abase::Rand(0,counter-1);
			pos = _town_list[list[index]].target_pos;
			tag = GetWorldTag();
			return true;
		}
		return false;
	}
public:
	countrybattle_world_manager():instance_world_manager()
	{
		//ս������Ӧ���ǹ̶�ʱ�����
		_idle_time = 300;
		_life_time = -1;
		_battle_type = -1;
		_flag_mine_tid = 0;
		memset(&_attacker_flag_goal, 0, sizeof(_attacker_flag_goal));
		memset(&_defender_flag_goal, 0, sizeof(_defender_flag_goal));
	}
	virtual int GetWorldType(){ return WORLD_TYPE_COUNTRYBATTLE; }
	virtual world * CreateWorldTemplate();
	virtual world_message_handler * CreateMessageHandler();
	virtual void Heartbeat();
	virtual void PreInit(const char * servername);
	virtual void FinalInit(const char * servername);
	virtual void OnDeliveryConnected();
	virtual bool IsCountryBattleWorld(){ return true; }
	virtual void NotifyCountryBattleConfig(GMSV::CBConfig * config);
	virtual int OnMobDeath(world * pPlane, int faction,  int tid);
	virtual int OnMineGathered(world * pPlane, int mine_tid, gplayer* pPlayer);
	virtual int GenerateFlag();
	virtual bool IsReachFlagGoal(bool offense, const A3DVECTOR& pos);
	virtual bool CanBeGathered(int player_faction, int mine_tid);
	const TOWER_LIST & GetTowerList(bool offense);
	const STRONGHOLD_LIST & GetStrongholdList(){ return _stronghold_list; }

	virtual void UserLogin(int cs_index,int cs_sid,int uid,const void * auth_data, size_t auth_size, bool isshielduser, char flag);
	virtual void SetFilterWhenLogin(gplayer_imp * pImp, instance_key * ikey);
	virtual void GetLogoutPos(gplayer_imp * pImp, int & world_tag ,A3DVECTOR & pos);
	virtual bool GetTownPosition(gplayer_imp *pImp, const A3DVECTOR &opos, A3DVECTOR &pos, int & tag);
	virtual void RecordTownPos(const A3DVECTOR &pos,int faction);
	virtual void SetIncomingPlayerPos(gplayer * pPlayer, const A3DVECTOR & origin_pos, int special_mask);
	
	virtual void TransformInstanceKey(const instance_key::key_essence & key, instance_hash_key & hkey)
	{
		hkey.key1 = key.key_level4;
		hkey.key2 = 0;
	}
	virtual int CheckPlayerSwitchRequest(const XID & who,const instance_key * key,const A3DVECTOR & pos,int ins_timer);
	virtual world * GetWorldInSwitch(const instance_hash_key & ikey,int & world_index,int );
	virtual bool CreateCountryBattle(const country_battle_param &);
	virtual void DestroyCountryBattle(int battleid);
};

class countrybattle_world_message_handler : public instance_world_message_handler
{
protected:
	virtual ~countrybattle_world_message_handler(){}
	virtual void PlayerPreEnterServer(gplayer * pPlayer, gplayer_imp * pimp,instance_key &  ikey);//�ڵ���EnterWorld֮ǰ�Ĵ���
public:
	countrybattle_world_message_handler(instance_world_manager * man):instance_world_message_handler(man) {}
	virtual int HandleMessage(world * pPlane, const MSG& msg);
	virtual int RecvExternMessage(int msg_tag,const MSG & msg);
};

#endif