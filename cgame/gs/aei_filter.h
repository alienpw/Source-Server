#ifndef __ONLINE_GAME_AUTO_ESCAPE_INSTANCE_FILTER_H__
#define __ONLINE_GAME_AUTO_ESCAPE_INSTANCE_FILTER_H__

#include "filter.h"
#include "worldmanager.h"
#include "faction.h"

//�����ж�����Ƿ�Ӧ���ѳ�������filter

enum KICKOUT_INSTANCE_REASON
{
	KIR_NULL 				= 0,	//ȡ���߳�	
	KIR_KEY_MISMATCH,				//����key��ƥ��
	KIR_PLAYERCOUNT_EXCEED,			//�����д��ڵ�ǰ����������������
	KIR_BATTLE_END,					//��վ����
	KIR_FACTION_FORTRESS_CLEAR,		//���ɻ����峡
	KIR_FACTION_MISMATCH,			//���ɲ�ƥ��
	KIR_GLOBALWORLD_KICKOUT,		//GLOBALWORLD���߳�����
	KIR_NO_COUNTRY,					//û�м������
	KIR_COUNTRYBATTLE_END,			//��սս������
	KIR_TRICKBATTLE_END,			//ս��ս������
};

class aei_filter : public filter
{
	int _state;
	int _timeout;
	instance_hash_key _key;

	enum
	{	
		NORMAL,
		WAIT_ESCAPE,
	};

	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
public:
	DECLARE_SUBSTANCE(aei_filter);
	aei_filter(gactive_imp * imp,int filter_id)
		:filter(object_interface(imp),MASK),_state(0), _timeout(0)
	{
		_filter_id = filter_id;
	}

protected:
	virtual void OnAttach();
	virtual void OnRelease(); 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aei_filter() {}
};

class aebf_filter : public filter
{
	int _timeout;
	int _origin_mafia;
	int _battle_result;
	int _battle_end_timer;
	int _attacker_score;
	int _defender_score;
	int _faction;
	int _kickout;
	int _attack_defend_award;

	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
public:
	DECLARE_SUBSTANCE(aebf_filter);
	aebf_filter(gactive_imp * imp,int filter_id, int origin_mafia, int ada)
		:filter(object_interface(imp),MASK),_timeout(0),_origin_mafia(origin_mafia),
		 _attack_defend_award(ada)
	{
		_filter_id = filter_id;
		_battle_result = 0;
		_battle_end_timer = 0;
		_attacker_score = -1;
		_defender_score = -1;
		_faction = _parent.GetFaction();
		_kickout = 0;
	}

protected:
	virtual void OnAttach();
	virtual void OnRelease(); 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aebf_filter():_timeout(0),_origin_mafia(0) {}
	virtual void  OnModify(int ctrlname,void * ctrlval,size_t ctrllen);
};

class aeff_filter : public filter
{
	int _state;
	int _timeout;
	int _kickout;
	enum
	{	
		NORMAL,
		WAIT_ESCAPE,
	};
	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
	
public:
	DECLARE_SUBSTANCE(aeff_filter);
	aeff_filter(gactive_imp * imp,int filter_id)
		:filter(object_interface(imp),MASK),_state(NORMAL),_timeout(0),_kickout(0)
	{
		_filter_id = filter_id;
	}

protected:
	virtual void OnAttach(){}
	virtual void OnRelease(){} 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aeff_filter(){}
	virtual void  OnModify(int ctrlname,void * ctrlval,size_t ctrllen);
};

class aegw_filter : public filter
{
	int _state;
	int _timeout;
	int _kickout;
	enum
	{	
		NORMAL,
		WAIT_ESCAPE,
	};
	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
	
public:
	DECLARE_SUBSTANCE(aegw_filter);
	aegw_filter(gactive_imp * imp,int filter_id)
		:filter(object_interface(imp),MASK),_state(NORMAL),_timeout(0),_kickout(0)
	{
		_filter_id = filter_id;
	}

protected:
	virtual void OnAttach(){}
	virtual void OnRelease(){} 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aegw_filter(){}
};

class aect_filter : public filter
{
	int _state;
	int _timeout;
	int _kickout;
	enum
	{	
		NORMAL,
		WAIT_ESCAPE,
	};
	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
	
public:
	DECLARE_SUBSTANCE(aect_filter);
	aect_filter(gactive_imp * imp,int filter_id)
		:filter(object_interface(imp),MASK),_state(NORMAL),_timeout(0),_kickout(0)
	{
		_filter_id = filter_id;
	}

protected:
	virtual void OnAttach(){}
	virtual void OnRelease(){} 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aect_filter(){}
};

class aecb_filter : public filter
{
	int _timeout;
	int _origin_country;
	int _battle_result;
	int _battle_end_timer;
	int _attacker_score;
	int _defender_score;
	int _attacker_count;
	int _defender_count;
	int _kickout;

	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
public:
	DECLARE_SUBSTANCE(aecb_filter);
	aecb_filter(gactive_imp * imp,int filter_id, int origin_country)
		:filter(object_interface(imp),MASK),_timeout(0),_origin_country(origin_country)
	{
		_filter_id = filter_id;
		_battle_result = 0;
		_battle_end_timer = 0;
		_attacker_score = -1;
		_defender_score = -1;
		_attacker_count = -1;
		_defender_count = -1;
		_kickout = 0;
	}

protected:
	virtual void OnAttach(){}
	virtual void OnRelease(){} 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aecb_filter():_timeout(0),_origin_country(0) {}
	virtual void  OnModify(int ctrlname,void * ctrlval,size_t ctrllen);
};

class aetb_filter : public filter
{
	int _timeout;
	int _kickout;
	int _battle_result;
	int _err_condition;

	enum 
	{
		MASK = FILTER_MASK_HEARTBEAT | FILTER_MASK_NOSAVE | FILTER_MASK_UNIQUE 
	};
public:
	DECLARE_SUBSTANCE(aetb_filter);
	aetb_filter(gactive_imp * imp,int filter_id)
		:filter(object_interface(imp),MASK),_timeout(0),_kickout(0),_battle_result(0),_err_condition(0)
	{
		_filter_id = filter_id;
	}

protected:
	virtual void OnAttach(){}
	virtual void OnRelease(){} 
	virtual void Heartbeat(int tick);
	virtual bool Save(archive & ar)
	{
		ASSERT(false);
		return true;
	}

	virtual bool Load(archive & ar)
	{
		ASSERT(false);
		return false;
	}
	
	aetb_filter(){}
	virtual void  OnModify(int ctrlname,void * ctrlval,size_t ctrllen);
};
#endif
