#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arandomgen.h>
#include "string.h"
#include "iconv.h"

#include <common/protocol.h>
#include "common/message.h"

#include "obj_interface.h"
#include <stocklib.h>
#include "world.h"
#include "player_imp.h"
#include "usermsg.h"
#include "clstab.h"
#include "actsession.h"
#include "userlogin.h"
#include "playertemplate.h"
#include "serviceprovider.h"
#include <common/protocol_imp.h>
#include "trade.h"
#include "task/taskman.h"
#include "sitdown_filter.h"
#include "playerstall.h"
#include "pvplimit_filter.h"
#include "pk_protected_filter.h"
#include <glog.h>
#include "pathfinding/pathfinding.h"
#include "player_mode.h"
#include "cooldowncfg.h"
#include "item/item_petegg.h"
#include "template/globaldataman.h"
#include "petnpc.h"
#include "antiwallow.h"
#include "../common/chatdata.h"
#include "teamrelationjob.h"
#include "instance/faction_world_ctrl.h"
#include <base64.h>
#include <db_if.h>
#include "rune_filter.h"
#include "item/generalcard_set_man.h"
#include "item/item_generalcard.h"
#include "template/el_region.h"

#include "global_controller.h"


DEFINE_SUBSTANCE_ABSTRACT(switch_additional_data, substance, CLS_SWITCH_ADDITIONAL_DATA);
DEFINE_SUBSTANCE(countryterritory_switch_data, switch_additional_data, CLS_COUNTRYTERRITORY_SWITCH_DATA);
DEFINE_SUBSTANCE(trickbattle_switch_data, switch_additional_data, CLS_TRICKBATTLE_SWITCH_DATA);

DEFINE_SUBSTANCE(gplayer_imp,gobject_imp,CLS_PLAYER_IMP)
DEFINE_SUBSTANCE(gplayer_dispatcher,dispatcher,CLS_PLAYER_DISPATCHER)

void TrySwapPlayerData(world * pPlane,const int cid[3],gplayer * pPlayer)
{
	__PRINTF("��ʼ������ҵ������û�\n");
	gplayer_imp * imp = (gplayer_imp*) pPlayer->imp;
	gplayer_controller * ctrl = (gplayer_controller *)imp->_commander;
	gplayer_dispatcher * dis = (gplayer_dispatcher*)imp->_runner;
	if(imp->GetGUID() == cid[0] &&
			ctrl->GetGUID() == cid[1] &&
			dis->GetGUID() == cid[2])
	{
		//��ԭ��������ȫһ�£����봦��
		return ;
	}

	//�������� ǰ��Ӧ���Ѿ���֤��cid����ȷ����
	gplayer_imp * new_imp = (gplayer_imp *)CF_Create(cid[0],cid[2],cid[1],pPlane,pPlayer);
	ASSERT(new_imp);
//	gplayer_controller * new_ctrl =(gplayer_controller*)new_imp->_commander; 
//	gplayer_dispatcher * new_dis = (gplayer_dispatcher*)new_imp->_runner;
	
	new_imp->Swap(imp);
//	new_ctrl->LoadFrom(ctrl); �������Ѿ�Swap����
//	new_dis->LoadFrom(dis);

	pPlayer->imp = new_imp;
	delete  imp;
	delete  dis;
	delete  ctrl;
	return ;
}

gplayer_imp::gplayer_imp()
	:_inventory(item::INVENTORY,ITEM_LIST_BASE_SIZE),
	_equipment(item::BODY,item::EQUIP_INVENTORY_COUNT),
	_task_inventory(item::TASK_INVENTORY,TASKITEM_LIST_SIZE),
	_player_money(0),_player_state(PLAYER_STATE_NORMAL),_combat_timer(0),
	_player_title(this),_player_dailysign(this),_player_fatering(this),_trashbox(TRASHBOX_BASE_SIZE,TRASHBOX_BASE_SIZE4),
    _user_trashbox(0,0),_player_force(this),_player_reincarnation(this)
{
	memset(&_instance_switch_key,0,sizeof(_instance_switch_key));
	_inventory.SetOwner(this);
	_equipment.SetOwner(this);
	_task_inventory.SetOwner(this);
	_trashbox.SetOwner(this);
	_user_trashbox.SetOwner(this);

	_provider.id = XID(-1,-1);

	memset(&_basic,0,sizeof(_basic));
	memset(&_cur_prop,0,sizeof(_cur_prop));
	memset(&_base_prop,0,sizeof(_base_prop));

	_disconnect_timeout = 0;
	_offline_type = PLAYER_OFF_LOGOUT;

	_inv_level = 0;
	_money_capacity = MONEY_CAPACITY_BASE;
	_faction = 0;
	_enemy_faction = 0;
	_trade_obj = NULL;
	_stall_obj = NULL;
	_stall_trade_timer = 0;
	_stall_trade_id = g_timer.get_systime();
	_pvp_cooldown = 0;
	_write_timer = 513;
	_general_timeout = 0;
	_task_mask = 0;
	_link_notify_timer  = LINK_NOTIFY_TIMER;
	_cur_item.weapon_delay = UNARMED_ATTACK_DELAY;
	_trash_box_open_flag = false;
	_trash_box_open_view_only_flag = false;
	_user_trash_box_open_flag = false;
	_security_passwd_checked = false;
	_pvp_enable_flag = false;
	_force_attack = 0;
	_refuse_bless = 0;
	_tb_change_counter = 1;		//Ĭ������Ϊ�ֿ��������˱仯 �������
	_user_tb_change_counter = 1;//Ĭ������Ϊ�ʺŲֿ��������˱仯 �������
	_eq_change_counter = 1;		//Ĭ����Ϊװ���������˱仯 �������
	_kill_by_player = false;
	_free_pvp_mode = false;
	_nonpenalty_pvp_state = false;
	_resurrect_state = false;
	_resurrect_exp_reduce = 0.f;
	_resurrect_hp_factor = 0.f;
	_resurrect_mp_factor = 0.f;
	_resurrect_exp_lost_reduce = 0;
	_con_emote_target = 0;
	_con_emote_id = 0;
	_reputation = 0;
	_last_move_mode = 0;
	_logout_pos_flag = 0;
	_fall_counter = 0;
	_ap_per_hit = 0;
	_last_instance_tag = -1;
	_last_instance_timestamp = 0;
	_last_source_instance_tag = -1;
	_db_save_error = 0;
	memset(&move_checker,0,sizeof(move_checker));
	_username_len = 0;
	_pvp_combat_timer = 0;
	_double_exp_timeout = 0;
	_double_exp_mode = 0;
	_rest_counter_time = 0;
	_rest_time_used = 0;
	_rest_time_capacity = 0;
	_mafia_rest_time = 0;
	_mafia_rest_counter_time = 0;
	_login_timestamp = 0;
	_played_time = 0;
	_last_login_timestamp = 0;
	_create_timestamp = 0;
	_spec_task_reward = 0;
	_spec_task_reward2 = 0;
	_spec_task_reward_param = 0;
	_spec_task_reward_mask = 0;
	_duel_target = 0;
	_no_cooldown_mode = 0;
	_db_user_id = 0;

	_enemy_list.reserve(MAX_PLAYER_ENEMY_COUNT);

	_active_task_list.insert(_active_task_list.begin(),TASK_ACTIVE_LIST_BUF_SIZE,0);
	_finished_task_list.insert(_finished_task_list.begin(),TASK_FINISHED_LIST_BUF_SIZE,0);
	_finished_time_task_list.insert(_finished_time_task_list.begin(),TASK_FINISH_TIME_LIST_BUF_SIZE,0);
	_finish_task_count_list.insert(_finish_task_count_list.begin(),TASK_FINISH_COUNT_LIST_BUF_SIZE,0);
	_storage_task_list.insert(_storage_task_list.begin(),TASK_STORAGE_LIST_BUF_SIZE,0);

	_role_reputation_uchar.insert(_role_reputation_uchar.begin(),ROLE_REPUTATION_UCHAR_SIZE,0);
	_role_reputation_ushort.insert(_role_reputation_ushort.begin(),ROLE_REPUTATION_USHORT_SIZE,0);
	_role_reputation_uint.insert(_role_reputation_uint.begin(),ROLE_REPUTATION_UINT_SIZE,0);

	_speed_ctrl_factor = 16.0f;	//�����鲻��ĳ�ֵ����
	
	_mall_cash = 0;
	_mall_cash_used = 0;
	_mall_cash_offset = 0;
	_mall_cash_add = 0;
	_mall_order_id = 0;
	_mall_order_id_saved = 0;
	_mall_consumption = 0;
	_chat_emote = 0;
	_wallow_level = 0;
	_cheat_mode = 0;
	_cheat_punish = 0;
	_cheat_report = 0;

	_inv_switch_save_flag = false;
	_eqp_switch_save_flag = false;
	_tsk_switch_save_flag = false;

	_profit_time = 0;
	_profit_level = 0;
	_profit_timestamp = 0;
	_active_state_delay = 0;

	//lgc
	_min_elf_status_value = 0;
	memset(_equip_refine_level, 0, sizeof(_equip_refine_level));
	_soul_power = 0;
	_soul_power_en = 0;
	_min_addon_expire_date = 0;
	memset(&_pet_enhance,0,sizeof(_pet_enhance));
	memset(&_faction_contrib,0,sizeof(_faction_contrib));
	_level_up = false;
	_skill_attack_transmit_target = XID(-1,-1);
	_country_expire_time = 0;
	_in_central_server = false;
	_src_zoneid = 0;
	_king_expire_time = 0;
	_switch_additional_data = NULL;
	_need_refresh_equipment = false;
	_realm_exp = 0;
	_realm_level = 0;
	_leadership = 0;
	_leadership_occupied = 0;
	_world_contribution = 0;
	_world_contribution_cost = 0;
}

gplayer_imp::~gplayer_imp()
{
	if(_trade_obj)
	{
		//ASSERT(false && "���׶���Ӧ����ǰ�ͷŵ�");
		delete _trade_obj;
	}
	if(_stall_obj)
	{
		//ASSERT(false && "��̯����Ӧ����ǰ�ͷŵ�");
		delete _stall_obj;
	}
	ClearSwitchAdditionalData();
}

void 
gplayer_imp::Init(world * pPlane,gobject*parent)
{
	gactive_imp::Init(pPlane,parent);
	_team.Init(this);
	_invade_ctrl.Init(this);
	InitClock();
}

struct cl_world_contrib_reset : public clock_listener
{
	void OnClock(gplayer_imp* player,int type)	
	{ 
		player->ClearWorldContrib();
	}
	void OnPassClock(gplayer_imp* player,int type,int lastupdate,int now) 
	{ 
		player->ClearWorldContrib(); 
	}
};

struct cl_task_storage_refresh : public clock_listener
{
	void OnClock(gplayer_imp* player,int type)
	{
		PlayerTaskInterface  task_if(player); // ��0��ˢ�¹��׶�����,�������������Լ�ˢ��
		task_if.RefreshTaskStorage(0);
	}
	void OnPassClock(gplayer_imp* player,int type,int lastupdate,int now) {}
};

void
gplayer_imp::InitClock()
{
	/////////////////////////////////////////////////////////////////////////
	// declare
	static cl_world_contrib_reset cl_wcr;
	static cl_task_storage_refresh cl_tsr;
	/////////////////////////////////////////////////////////////////////////
	// registor
	_player_clock.AddNotice(&cl_wcr,player_clock::GPC_PER_HOUR_LOCAL,0); // ÿ��0�� ����
	_player_clock.AddNotice(&cl_tsr,player_clock::GPC_PER_HOUR_GLOBAL,0); // ÿ��0�� ȫ��
}

void 
gplayer_imp::PlayerEnterWorld()
{
	//����������ǰ������ʼ��
	for(size_t i = 0; i < _equipment.Size(); i ++)
	{
		item & it = _equipment[i];
		if(it.type == -1) continue;
		if(it.count == 0) continue;
		if(it.proc_type & item::ITEM_PROC_TYPE_BIND2)
		{
			//��װ����󶨵���Ʒ������ڰ����������Ѿ���
			it.proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
			it.proc_type &= ~(item::ITEM_PROC_TYPE_BIND2);

			UpdateMallConsumptionBinding(it.type, it.proc_type, it.count);
		}
	}

	//������е����ߵ���װ�����Է���һ��
	_inventory.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_equipment.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_task_inventory.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack2().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack3().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack4().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_user_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);


	//ʹ��װ����Ч
	RefreshEquipment();
	//����װ�������ݣ������˿���
	CalcEquipmentInfo();

	if(!world_manager::GetWorldParam().pve_mode)
	{
		//������ȫ������PVP���أ�һ������������ֲ��������ְ�ȫ���Ļ�������filter�ر�PVP����
		//if(_basic.level > PVP_PROTECT_LEVEL) 
		{
			GetParent()->object_state |= gactive_object::STATE_PVPMODE;
			_pvp_enable_flag = true;
			_pvp_cooldown = PVP_STATE_COOLDOWN;
		}
	}

	_plane->SyncPlayerWorldGen((gplayer*)_parent);
	//���ͽ���������Ϣ��ȡ�����������
	_runner->notify_pos(_parent->pos);	//add by liuguichen,20130715
	EnterWorld();
	if(_layer_ctrl.IsFalling())
	{
		//�����������״̬�����ǿ�ʼ����
		//���ڲ��ٷ��͵������ݣ������ɿͻ�������ɴ˲�����
	}

	//�����Ƿ���ˮ��
	TestUnderWater();

	//�����Ƿ������ҿ��Ը���
	if(_parent->IsZombie() && _resurrect_state)
	{
		_runner->enable_resurrect_state(_resurrect_exp_reduce);
	}
	
	//�����Ƿ��ڰ�ȫ��
	TestSanctuary();
	
	if(!world_manager::GetWorldParam().pve_mode)
	{
		//�����pvp������,����Ƿ������ְ�ȫ����
		TestPKProtected();
	}

	_runner->server_config_data();

	_ph_control.Initialize(this);
	UpdatePlayerLayer();	
	
	if(!_cooldown.TestCoolDown(COOLDOWN_INDEX_CHEATER))
	{
		_cheat_punish = 1;
	}

	PlaneEnterNotify(true);
	_runner->send_world_life_time(_plane->w_life_time);

	if(GetCountryId()) GMSV::CountryBattleOnline(_parent->ID.id, GetCountryId(), world_manager::GetWorldTag(), GetSoulPower(), GetParent()->IsKing());
}

void 
gplayer_imp::GetCommonDataList(bool send_content)
{
	packet_wrapper  h1(8192);
	using namespace S2C;
	CMD::Make<CMD::common_data_list>::From(h1);
	if(send_content){
		class my_stream : public common_data::stream
		{
			packet_wrapper  & _wrapper;
			int _count;
			public:
			my_stream(packet_wrapper & wrapper):_wrapper(wrapper),_count(0)
			{
			}
			void dump(int key, int value)
			{
				if(_count < 2048)
				{
					_count ++;
					_wrapper<< key << value;
				}
			}
		};

		my_stream t1(h1);
		_plane->w_common_data.Dump(100000, &t1);
	}
	send_ls_msg(GetParent(), h1);
}

int 
gplayer_imp::DispatchMessage(world *pPlane, const MSG & msg)
{
/*
	timeval t1,t2;
	gettimeofday(&t1,NULL);
	*/
	gplayer * pPlayer = (gplayer *)_parent;
	int rst = 0;
	switch(pPlayer->login_state)
	{
		case gplayer::WAITING_LOGOUT:
		case gplayer::WAITING_LOGIN:
		//�������κ���Ϣ
			break;
		case gplayer::WAITING_ENTER:
			if(msg.message == GM_MSG_ENTER_WORLD)
			{
				_general_timeout = 0; 
				pPlayer->login_state = gplayer::LOGIN_OK;
				PlayerEnterWorld();


				int locktime = msg.param;
				int maxlocktime =  0;
				if(msg.content_length == sizeof(int)) maxlocktime =  *(int*)msg.content;
				//��¼����ʱ��
				gplayer_controller * pCtrl = (gplayer_controller *) _commander;
				pCtrl->SetSafeLock(locktime, maxlocktime);
			}
			else
			if(msg.message == GM_MSG_HEARTBEAT)
			{
				_general_timeout ++;
				if(_general_timeout > 600)
				{
					//WAITING_ENTER 10 ���ӳ�ʱ
					int cs_index = ((gplayer*)_parent)->cs_index;
					int uid = ((gplayer*)_parent)->ID.id;
					int sid = ((gplayer*)_parent)->cs_sid;
					_commander->Release();
					GMSV::SendDisconnect(cs_index,uid,sid,0);
				}
			}
			break;

		case gplayer::DISCONNECTED:
		case gplayer::LOGIN_OK:
		{
			switch(_player_state)
			{
				case PLAYER_STATE_NORMAL:
				case PLAYER_STATE_COSMETIC:
				case PLAYER_STATE_BIND:
				rst = DispatchNormalMessage(pPlane,msg);
				break;

				case PLAYER_DISCONNECT:
				rst = DisconnectMessageHandler(pPlane,msg);
				break;

				case PLAYER_WAITING_TRADE:
				rst = WaitingTradeMessageHandler(pPlane,msg);
				break;

				case PLAYER_TRADE:
				rst = TradeMessageHandler(pPlane,msg);
				break;

				case PLAYER_WAIT_TRADE_COMPLETE:
				rst = WaitingTradeCompleteHandler(pPlane,msg);
				break;
					
				case PLAYER_WAIT_TRADE_READ:
				rst = WaitingTradeReadHandler(pPlane,msg);
				break;
				
				case PLAYER_WAIT_FACTION_TRADE_READ:
				rst = WatingFactionTradeReadHandler(pPlane,msg);
				break;

				case PLAYER_WAITING_FACTION_TRADE:
				rst = WaitingFactionTradeTradeHandler(pPlane,msg);
				break;

				case PLAYER_WAIT_SWITCH:
				rst = WaitingSwitchServer(pPlane,msg);
				break;

				case PLAYER_SIT_DOWN:
				rst = StayInHandler(pPlane,msg);
				break;

				case PLAYER_STATE_MARKET:
				rst = MarketHandler(pPlane,msg);
				break;

				case PLAYER_STATE_TRAVEL:
				rst = TravelMessageHandler(pPlane,msg);
				break;

				default:
				ASSERT(false);
				return 0;
			}
			break;
		}
		default:
			ASSERT(false);
			break;
	}
	/*
	gettimeofday(&t2,NULL);
	long t = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);
	__PRINTF("player message message %4d use ---------------%6ld\n",msg.message,t);
	*/
	return rst;
}

int
gplayer_imp::StayInHandler(world * pPlane ,const MSG & msg)
{
	switch(msg.message)
	{
		case GM_MSG_HEARTBEAT:
		case GM_MSG_QUERY_OBJ_INFO00:
		case GM_MSG_PICKUP_MONEY:
		case GM_MSG_PICKUP_ITEM:
		case GM_MSG_ERROR_MESSAGE:
		case GM_MSG_GROUP_EXPERIENCE:
		case GM_MSG_EXPERIENCE:
		case GM_MSG_TEAM_EXPERIENCE:
	//	case GM_MSG_GET_MEMBER_POS:
		case GM_MSG_TEAM_INVITE:
		case GM_MSG_TEAM_AGREE_INVITE:
		case GM_MSG_TEAM_REJECT_INVITE:
		case GM_MSG_TEAM_PICKUP:
		case GM_MSG_JOIN_TEAM:
		case GM_MSG_LEADER_UPDATE_MEMBER:
		case GM_MSG_JOIN_TEAM_FAILED:
		case GM_MSG_MEMBER_NOTIFY_DATA:
		case GM_MSG_NEW_MEMBER:
		case GM_MSG_LEAVE_PARTY_REQUEST:
		case GM_MSG_LEADER_CANCEL_PARTY:
		case GM_MSG_MEMBER_NOT_IN_TEAM:
		case GM_MSG_LEADER_KICK_MEMBER:
		case GM_MSG_MEMBER_LEAVE:
		case GM_MSG_QUERY_PLAYER_EQUIPMENT:
		case GM_MSG_PICKUP_TEAM_MONEY:
		case GM_MSG_RECEIVE_MONEY:
		case GM_MSG_NPC_BE_KILLED:
		case GM_MSG_HATE_YOU:
		case GM_MSG_PLAYER_TASK_TRANSFER:
		case GM_MSG_PLAYER_BECOME_PARIAH:
		case GM_MSG_PLAYER_BECOME_INVADER:
		case GM_MSG_NOTIFY_SELECT_TARGET:
		case GM_MSG_QUERY_SELECT_TARGET:
		case GM_MSG_SUBSCIBE_TARGET:
		case GM_MSG_UNSUBSCIBE_TARGET:
		case GM_MSG_SUBSCIBE_CONFIRM:
		case GM_MSG_SUBSCIBE_SUBTARGET:
		case GM_MSG_UNSUBSCIBE_SUBTARGET:
		case GM_MSG_SUBSCIBE_SUBTARGET_CONFIRM:
		case GM_MSG_NOTIFY_SELECT_SUBTARGET:
		case GM_MSG_HP_STEAL:
		case GM_MSG_TEAM_APPLY_PARTY:
		case GM_MSG_TEAM_APPLY_REPLY:
		case GM_MSG_QUERY_INFO_1:
		case GM_MSG_TEAM_CHANGE_TO_LEADER:
		case GM_MSG_TEAM_LEADER_CHANGED:
		//û�д���session����Ϣ��Ŀǰ������״̬��û���κ�session�����
		case GM_MSG_GM_RECALL:
		case GM_MSG_GM_CHANGE_EXP:
		case GM_MSG_GM_ENDUE_ITEM:
		case GM_MSG_GM_ENDUE_SELL_ITEM:
		case GM_MSG_GM_REMOVE_ITEM:
		case GM_MSG_GM_ENDUE_MONEY:
		case GM_MSG_GM_OFFLINE:
		case GM_MSG_GM_MQUERY_MOVE_POS:
		case GM_MSG_GM_MQUERY_MOVE_POS_REPLY:
		case GM_MSG_GM_DEBUG_COMMAND:
		case GM_MSG_GM_RESET_PP:
		case GM_MSG_GM_QUERY_SPEC_ITEM:
		case GM_MSG_GM_REMOVE_SPEC_ITEM:
		case GM_MSG_DBSAVE_ERROR:	
		case GM_MSG_ENABLE_PVP_DURATION:
		case GM_MSG_PLAYER_DUEL_REQUEST:
		case GM_MSG_PLAYER_DUEL_REPLY:
		case GM_MSG_PLAYER_DUEL_PREPARE:
		case GM_MSG_PLAYER_DUEL_START:
		case GM_MSG_PLAYER_DUEL_CANCEL:
		case GM_MSG_PLAYER_DUEL_STOP:
		case GM_MSG_QUERY_EQUIP_DETAIL:
		case GM_MSG_PLAYER_RECALL_PET:
		case GM_MSG_MOB_BE_TRAINED:
		case GM_MSG_PET_SET_COOLDOWN:
		case GM_MSG_PET_ANTI_CHEAT:
		case GM_MSG_PET_NOTIFY_DEATH:
		case GM_MSG_PET_NOTIFY_HP:
		case GM_MSG_PET_RELOCATE_POS:
		case GM_MSG_QUERY_PROPERTY:
		case GM_MSG_QUERY_PROPERTY_REPLY:
		case GM_MSG_NOTIFY_CLEAR_INVISIBLE:
		case GM_MSG_PLANT_PET_NOTIFY_DEATH:
		case GM_MSG_PLANT_PET_NOTIFY_HP:
		case GM_MSG_PLANT_PET_NOTIFY_DISAPPEAR:
		case GM_MSG_CONGREGATE_REQUEST:
		case GM_MSG_REJECT_CONGREGATE:
		case GM_MSG_NPC_BE_KILLED_BY_OWNER:
		case GM_MSG_EXTERN_HEAL:
		case GM_MSG_QUERY_INVENTORY_DETAIL:
		case GM_MSG_PLAYER_KILLED_BY_PLAYER:
		case GM_MSG_PUNISH_ME:
		case GM_MSG_REDUCE_CD:	
		//���µ�ʱ����Դ���GM��Ϣ
			return MessageHandler(pPlane,msg);

		//�ܵ��е���ķ���Ҳ�ᴦ��
		case GM_MSG_ENCHANT:
			if(!((enchant_msg*)msg.content)->helpful)
			{
				LeaveStayInState();
			}
			return MessageHandler(pPlane,msg);
		case GM_MSG_ATTACK:
		case GM_MSG_HURT:
		case GM_MSG_DUEL_HURT:
		case GM_MSG_TRANSFER_FILTER_DATA:
		case GM_MSG_TRANSFER_FILTER_GET:
		//�ܵ������ᵼ���뿪ͣ��״̬
			LeaveStayInState();
			return MessageHandler(pPlane,msg);

			
	}
	return 0;
}

int
gplayer_imp::TravelMessageHandler(world * pPlane ,const MSG & msg)
{
	switch(msg.message)
	{
		case GM_MSG_HEARTBEAT:
		case GM_MSG_QUERY_OBJ_INFO00:
		case GM_MSG_PICKUP_MONEY:
		case GM_MSG_PICKUP_ITEM:
		case GM_MSG_ERROR_MESSAGE:
		case GM_MSG_GROUP_EXPERIENCE:
		case GM_MSG_EXPERIENCE:
		case GM_MSG_TEAM_EXPERIENCE:
	//	case GM_MSG_GET_MEMBER_POS:
		case GM_MSG_TEAM_INVITE:
		case GM_MSG_TEAM_AGREE_INVITE:
		case GM_MSG_TEAM_REJECT_INVITE:
		case GM_MSG_TEAM_PICKUP:
		case GM_MSG_JOIN_TEAM:
		case GM_MSG_LEADER_UPDATE_MEMBER:
		case GM_MSG_JOIN_TEAM_FAILED:
		case GM_MSG_MEMBER_NOTIFY_DATA:
		case GM_MSG_NEW_MEMBER:
		case GM_MSG_LEAVE_PARTY_REQUEST:
		case GM_MSG_LEADER_CANCEL_PARTY:
		case GM_MSG_MEMBER_NOT_IN_TEAM:
		case GM_MSG_LEADER_KICK_MEMBER:
		case GM_MSG_MEMBER_LEAVE:
		case GM_MSG_QUERY_PLAYER_EQUIPMENT:
		case GM_MSG_QUERY_EQUIP_DETAIL:
		case GM_MSG_PICKUP_TEAM_MONEY:
		case GM_MSG_RECEIVE_MONEY:
		case GM_MSG_NPC_BE_KILLED:
		case GM_MSG_PLAYER_TASK_TRANSFER:
		case GM_MSG_PLAYER_BECOME_PARIAH:
		case GM_MSG_PLAYER_BECOME_INVADER:
		case GM_MSG_NOTIFY_SELECT_TARGET:
		case GM_MSG_QUERY_SELECT_TARGET:
		case GM_MSG_SUBSCIBE_TARGET:
		case GM_MSG_UNSUBSCIBE_TARGET:
		case GM_MSG_SUBSCIBE_CONFIRM:
		case GM_MSG_SUBSCIBE_SUBTARGET:
		case GM_MSG_UNSUBSCIBE_SUBTARGET:
		case GM_MSG_SUBSCIBE_SUBTARGET_CONFIRM:
		case GM_MSG_NOTIFY_SELECT_SUBTARGET:
		case GM_MSG_HP_STEAL:
		case GM_MSG_TEAM_APPLY_PARTY:
		case GM_MSG_TEAM_APPLY_REPLY:
		case GM_MSG_QUERY_INFO_1:
		case GM_MSG_TEAM_CHANGE_TO_LEADER:
		case GM_MSG_TEAM_LEADER_CHANGED:
		case GM_MSG_QUERY_PROPERTY:	
		case GM_MSG_QUERY_PROPERTY_REPLY:	

		//����session,��ʵ������ֻ������·��session
		case GM_MSG_OBJ_SESSION_END:
		case GM_MSG_OBJ_SESSION_REPEAT:
		case GM_MSG_OBJ_SESSION_REPEAT_FORCE:
		case GM_MSG_DBSAVE_ERROR:


		//����GM��Ϣ
		case GM_MSG_GM_RECALL:
		case GM_MSG_GM_CHANGE_EXP:
		case GM_MSG_GM_ENDUE_ITEM:
		case GM_MSG_GM_ENDUE_SELL_ITEM:
		case GM_MSG_GM_REMOVE_ITEM:
		case GM_MSG_GM_ENDUE_MONEY:
		case GM_MSG_GM_OFFLINE:
		case GM_MSG_GM_DEBUG_COMMAND:
		case GM_MSG_GM_MQUERY_MOVE_POS:
		case GM_MSG_GM_MQUERY_MOVE_POS_REPLY:
		case GM_MSG_GM_RESET_PP:
		case GM_MSG_GM_QUERY_SPEC_ITEM:
		case GM_MSG_GM_REMOVE_SPEC_ITEM:
		case GM_MSG_ENABLE_PVP_DURATION:
			return MessageHandler(pPlane,msg);

		//�������й�������Ϣ�ͼ�����Ϣ
		case GM_MSG_ENCHANT:
		case GM_MSG_ATTACK:
		case GM_MSG_HURT:
		case GM_MSG_DUEL_HURT:
			return 0;
			
	}
	return 0;
}

int 
gplayer_imp::ZombieMessageHandler(world * pPlane ,const MSG & msg)
{
	//ֻ��������
	switch(msg.message)
	{
		case GM_MSG_SWITCH_GET:
		//���账��
		break;

		case GM_MSG_GATHER_REPLY:
		{
			//ȡ���ɼ�
			SendTo<0>(GM_MSG_GATHER_CANCEL,msg.source,0);
		}
		return 0;

		case GM_MSG_OBJ_ZOMBIE_SESSION_END:
		case GM_MSG_GATHER_RESULT:
		case GM_MSG_HEARTBEAT:
		case GM_MSG_QUERY_OBJ_INFO00:
		case GM_MSG_PICKUP_MONEY:
		case GM_MSG_PICKUP_ITEM:
		case GM_MSG_ERROR_MESSAGE:
		case GM_MSG_GROUP_EXPERIENCE:
//		case GM_MSG_EXPERIENCE:
//		case GM_MSG_TEAM_EXPERIENCE:
//		case GM_MSG_GET_MEMBER_POS:
		case GM_MSG_TEAM_INVITE:
		case GM_MSG_TEAM_AGREE_INVITE:
		case GM_MSG_TEAM_REJECT_INVITE:
		case GM_MSG_TEAM_PICKUP:
		case GM_MSG_JOIN_TEAM:
		case GM_MSG_LEADER_UPDATE_MEMBER:
		case GM_MSG_JOIN_TEAM_FAILED:
		case GM_MSG_MEMBER_NOTIFY_DATA:
		case GM_MSG_NEW_MEMBER:
		case GM_MSG_LEAVE_PARTY_REQUEST:
		case GM_MSG_LEADER_CANCEL_PARTY:
		case GM_MSG_MEMBER_NOT_IN_TEAM:
		case GM_MSG_LEADER_KICK_MEMBER:
		case GM_MSG_MEMBER_LEAVE:
		case GM_MSG_QUERY_PLAYER_EQUIPMENT:
		case GM_MSG_PICKUP_TEAM_MONEY:
//		case GM_MSG_RECEIVE_MONEY:
		case GM_MSG_NPC_BE_KILLED:
		case GM_MSG_PLAYER_TASK_TRANSFER:
		case GM_MSG_PLAYER_BECOME_PARIAH:
		case GM_MSG_PLAYER_BECOME_INVADER:
		case GM_MSG_NOTIFY_SELECT_TARGET:
		case GM_MSG_QUERY_SELECT_TARGET:
		case GM_MSG_SUBSCIBE_TARGET:
		case GM_MSG_UNSUBSCIBE_TARGET:
		case GM_MSG_SUBSCIBE_CONFIRM:
		case GM_MSG_SUBSCIBE_SUBTARGET:
		case GM_MSG_UNSUBSCIBE_SUBTARGET:
		case GM_MSG_SUBSCIBE_SUBTARGET_CONFIRM:
		case GM_MSG_NOTIFY_SELECT_SUBTARGET:
		case GM_MSG_TEAM_APPLY_PARTY:
		case GM_MSG_TEAM_APPLY_REPLY:
		case GM_MSG_QUERY_INFO_1:
		case GM_MSG_TEAM_CHANGE_TO_LEADER:
		case GM_MSG_TEAM_LEADER_CHANGED:
		case GM_MSG_DBSAVE_ERROR:
		case GM_MSG_PLAYER_DUEL_START:
		case GM_MSG_PLAYER_DUEL_CANCEL:
		case GM_MSG_PLAYER_DUEL_STOP:

		case GM_MSG_GM_RECALL:
		case GM_MSG_PLANE_SWITCH_REPLY:
		case GM_MSG_ENABLE_PVP_DURATION:
		case GM_MSG_QUERY_EQUIP_DETAIL:
		case GM_MSG_REMOVE_ITEM:
		case GM_MSG_QUERY_PROPERTY:
		case GM_MSG_QUERY_PROPERTY_REPLY:
		case GM_MSG_CONTRIBUTION_TO_KILL_NPC:
		case GM_MSG_GROUP_CONTRIBUTION_TO_KILL_NPC:
		case GM_MSG_CONGREGATE_REQUEST:
		case GM_MSG_REJECT_CONGREGATE:
		case GM_MSG_NPC_BE_KILLED_BY_OWNER:
		case GM_MSG_QUERY_INVENTORY_DETAIL:
		case GM_MSG_PLAYER_KILLED_BY_PLAYER:
		
		//�ܹ�������GM��Ϣ
		case GM_MSG_GM_MQUERY_MOVE_POS:
		case GM_MSG_GM_MQUERY_MOVE_POS_REPLY:
		case GM_MSG_GM_OFFLINE:
		case GM_MSG_GM_QUERY_SPEC_ITEM:
		case GM_MSG_GM_REMOVE_SPEC_ITEM:

		//��Щ��Ϣ�Ǻͻ��ŵ�ʱ��ӵ��һ���Ĵ���
		return MessageHandler(pPlane,msg);

		case GM_MSG_GM_RESURRECT:
		{
		//$$$$$$$$$�Ժ������µ���Ϣ
		//	Resurrect(0.f);
			gplayer_controller * pCtrl = (gplayer_controller *)(_commander);
			pCtrl->ResurrectInTown(0.f,msg.param);
		}
		return 0;

		case GM_MSG_SCROLL_RESURRECT:
		{
			if(!msg.param && _invader_state != INVADER_LVL_0)  
			{
				//��PVPģʽ�Ķ����ܸ�������ͷ���
				return 0;
			}
			EnterResurrectReadyState(0.05f,DEFAULT_RESURRECT_HP_FACTOR,DEFAULT_RESURRECT_MP_FACTOR);
		};
		return 0;

		case GM_MSG_ENCHANT_ZOMBIE:
		{
			__PRINTF("recv zombie enchant\n");
			ASSERT(msg.content_length >= sizeof(enchant_msg));
			enchant_msg ech_msg = *(enchant_msg*)msg.content;
			if(!ech_msg.helpful)
			{
				return 0;
			}
			else
			{
				if(!TestHelpfulEnchant(msg,ech_msg)) return 0;
			}
			HandleEnchantMsg(pPlane,msg,&ech_msg);
		}
		return 0;

	}
	return 0;
}

void 
gplayer_imp::CancelSwitch()
{
	if(_player_state == PLAYER_WAIT_SWITCH)
	{
		FromSwitchToNormal();
	}
}

int 
gplayer_imp::WaitingSwitchServer(world * pPlane, const MSG & msg)
{
	//ֻ��������
	switch(msg.message)
	{
		case GM_MSG_SWITCH_GET:
		__PRINTF("�ܵ�ת�Ʒ���������Ϣ\n");
		return DoSwitch(msg);

		case GM_MSG_SWITCH_FAILED:
		{
			_runner->error_message(msg.param);
			gplayer *pPlayer = GetParent();
			GMSV::SendSwitchServerCancel(pPlayer->cs_index,pPlayer->ID.id, pPlayer->cs_sid);
			//�ص���ͨ״̬
			__PRINTF("ת�Ʒ��������־ܾ� �� �ص�����״̬\n");

			FromSwitchToNormal();
			return 0;
		}

		case GM_MSG_HEARTBEAT:
		if(--_general_timeout<=0)
		{
			//״̬��ʱ��
			gplayer *pPlayer = GetParent();
			GMSV::SendSwitchServerCancel(pPlayer->cs_index,pPlayer->ID.id, pPlayer->cs_sid);
			//�ص���ͨ״̬
			__PRINTF("�ȴ�ת�Ʒ�����ʱ��ʱ���ص�����״̬\n");

			FromSwitchToNormal();
		}
		default:
		return DispatchNormalMessage(pPlane,msg);
	}
}

int 
gplayer_imp::MessageHandler(world * pPlane ,const MSG & msg)
{
	switch(msg.message)
	{
		case GM_MSG_ATTACK:
		{
			ASSERT(msg.content_length >= sizeof(attack_msg));

			attack_msg ack_msg = *(attack_msg*)msg.content;
			//����һ�µ����Ĺ�����Ϣ
			_filters.EF_TransRecvAttack(msg.source, ack_msg);
			
			if(!TestAttackMsg(msg,ack_msg)) return 0;

			if(TryTransmitSkillAttack(msg)) return 0;
			//����ѡ�����
			((gplayer_controller*)_commander)->TrySelect(msg.source);
			HandleAttackMsg(pPlane,msg,&ack_msg);
		}
		return 0;

		case GM_MSG_ENCHANT:
		{
			ASSERT(msg.content_length >= sizeof(enchant_msg));
			enchant_msg ech_msg = *(enchant_msg*)msg.content;
			_filters.EF_TransRecvEnchant(msg.source, ech_msg);
			if(!ech_msg.helpful)
			{
				if(!TestHarmfulEnchant(msg,ech_msg)) return 0;
			}
			else
			{
				if(!TestHelpfulEnchant(msg,ech_msg)) return 0;
			}
			if(TryTransmitSkillAttack(msg)) return 0;
			HandleEnchantMsg(pPlane,msg,&ech_msg);
		}
		return 0;

		case GM_MSG_SWITCH_GET:
			return 0;
		case GM_MSG_ERROR_MESSAGE:
			_runner->error_message(msg.param);
			return 0;

		case GM_MSG_PICKUP_TEAM_MONEY:
			if(msg.content_length == sizeof(int))
			{
				int drop_id = *(int*)msg.content;
				if(drop_id)
				{
					GLog::log(LOG_INFO,"�û�%d��Ӽ����û�%d�����Ľ�Ǯ%d",_parent->ID.id,drop_id,msg.param);
				}
				if(msg.param > 0)
				{
					if(!_team.PickupTeamMoney(msg.pos,msg.param))
					{
						//����ַ�ʧ�� �������Լ���
						SendTo<0>(GM_MSG_RECEIVE_MONEY, GetParent()->ID, msg.param);
					}
				}
			}
			else
			{
				ASSERT(false);
			}
			return 0;
		case GM_MSG_TEAM_EXPERIENCE:
			if(msg.content_length == sizeof(msg_grp_exp_t)) 
			{
				if(msg.pos.squared_distance(_parent->pos) <= (TEAM_EXP_DISTANCE * TEAM_EXP_DISTANCE))
				{
					msg_grp_exp_t * pExp = (msg_grp_exp_t*)msg.content;
					int exp = pExp->exp;
					int sp = pExp->sp;
					if(world_manager::AntiWallow())
					{
						anti_wallow::AdjustNormalExpSP(_wallow_level, exp, sp);
					}
					//��ֹ����ʱ��Ϊ0�Ķ�Ա��ȡ����ֵ
					if(_profit_level != PROFIT_LEVEL_NONE)
					{
						ReceiveExp(exp,sp);
					}
					if(pExp->level > 0)
					{
						if(_task_mask & TASK_MASK_KILL_MONSTER)
						{
							PlayerTaskInterface  task_if(this);
							OnTaskTeamKillMonster(&task_if,msg.param, pExp->level,pExp->rand);
						}
						//�����ɱ�ù���,��������ӿ� 
					}
				}
			}
			else
			{
				ASSERT(false && "����ֵ��Ϣ�����ݴ�С����ȷ");
			}
			return 0;
		case GM_MSG_EXPERIENCE:
			if(msg.content_length == sizeof(msg_exp_t))
			{
				if(msg.pos.squared_distance(_parent->pos) <= (NORMAL_EXP_DISTANCE * NORMAL_EXP_DISTANCE))
				{
					ReceiveExp(*(msg_exp_t*)msg.content);
				}
			}
			else
			{
				ASSERT(false && "����ֵ��Ϣ�����ݴ�С����ȷ");
			}
			return 0;
		case GM_MSG_GROUP_EXPERIENCE:
			if(msg.content_length && (msg.content_length % sizeof(msg_grpexp_t)) == 0)
			{
				int count = msg.content_length / sizeof(msg_grpexp_t);
				ASSERT(msg.param > 0);
				ASSERT(count > 2);
				msg_grpexp_t * pExp = (msg_grpexp_t*)msg.content;
				if(_team.IsLeader() && _team.GetTeamSeq() == pExp->damage)
				{	
					//�ڵ�һ��Ԫ�����汣���˾���ֵ������ֵ��seq
					//�ڵڶ���Ԫ�����汣����world tag��ֵ
					//�����message.h
					msg_grpexp_t *pExp = (msg_grpexp_t *) msg.content;
					int exp = pExp->who.type;
					int sp = pExp->who.id & 0xFFFFFF;
					int level = (pExp->who.id >> 24) & 0x00FF;
					float r = *(float*)&(pExp[1].who.id);
					ReceiveGroupExp(msg.pos,msg.param,exp,sp,level,count-2,pExp[1].who.type, pExp[1].damage & 0xFFFF, (pExp[1].damage >> 16) & 0xFFFF, r, pExp+2);
				}
			}
			else
			{
				ASSERT(false && "����ֵ��Ϣ�����ݴ�С����ȷ");
			}
			return 0;
			

		case GM_MSG_HATE_YOU:
			ActiveCombatState(true);
			SetCombatTimer(NORMAL_COMBAT_TIME);
			((gplayer_controller*)_commander)->TrySelect(msg.source);

			if(_enemy_list.size() < MAX_PLAYER_ENEMY_COUNT)
			{
				_enemy_list[msg.source.id] ++;
			}
			else
			{
				ENEMY_LIST::iterator it = _enemy_list.find(msg.source.id);
				if(it != _enemy_list.end())
				{
					it->second ++;
				}
			}

			return 0;


		case GM_MSG_TEAM_INVITE:
			ASSERT(msg.content_length == sizeof(int));
			if(msg.content_length == sizeof(int) && !_team.MsgInvite(msg.source,msg.param,*(int*)msg.content))
			{
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_TEAM_CANNOT_INVITE);

			}
			return 0;

		case GM_MSG_TEAM_AGREE_INVITE:
			ASSERT(msg.content_length == sizeof(player_team::agree_invite_entry));
			if(msg.content_length == sizeof(player_team::agree_invite_entry))
			{
				_team.MsgAgreeInvite(msg.source,msg.pos,*(player_team::agree_invite_entry*)msg.content,msg.param);
			}
			return 0;

		case GM_MSG_TEAM_REJECT_INVITE:
			_team.MsgRejectInvite(msg.source);
			return 0;

		case GM_MSG_JOIN_TEAM:
			{
				int count = msg.param & 0x7FFF;
				int pickup_flag = (msg.param >> 16 ) & 0x7FFF;
				size_t header_size = count * sizeof(player_team::member_entry);
				ASSERT(msg.content_length == header_size + _cur_tag_counter.size()*sizeof(int)*3 + sizeof(int64_t));
				if(msg.content_length == header_size +  _cur_tag_counter.size()*sizeof(int)*3 + sizeof(int64_t))
				{
					
					void * buf = ((char*)msg.content) + header_size;
					int64_t team_uid = *((int64_t*)buf);
					buf = ((char*)msg.content) + header_size + sizeof(int64_t);
					size_t size = msg.content_length - header_size - sizeof(int64_t);
					_team.MsgJoinTeam(msg.source,(player_team::member_entry*)msg.content,
							count,pickup_flag,team_uid,buf,size);
				}
			}
			return 0;
		case GM_MSG_LEADER_UPDATE_MEMBER:
			ASSERT(msg.content_length == msg.param * sizeof(player_team::member_entry));
			if(msg.content_length == msg.param * sizeof(player_team::member_entry))
			{
				_team.MsgLeaderUpdateData(msg.source,
						(player_team::member_entry*)msg.content,msg.param);
			}
			return 0;

		case GM_MSG_JOIN_TEAM_FAILED:
			_team.MsgJoinTeamFailed(msg.source);
			return 0;

		case GM_MSG_MEMBER_NOTIFY_DATA:
			ASSERT(msg.content_length == sizeof(team_mutable_prop));
			if(msg.content_length == sizeof(team_mutable_prop))
			{
				_team.MsgMemberUpdateData(msg.source,msg.pos,*(team_mutable_prop*)msg.content);
			}
			return 0;
			
		case GM_MSG_NEW_MEMBER:
			ASSERT(msg.content_length == msg.param * sizeof(player_team::member_entry));
			if(msg.content_length == msg.param * sizeof(player_team::member_entry))
			{
				_team.MsgNewMember(msg.source,(player_team::member_entry*)msg.content,msg.param);
			}
			return 0;

		case GM_MSG_LEAVE_PARTY_REQUEST:
			_team.MsgMemberLeaveRequest(msg.source);
			return 0;

		case GM_MSG_LEADER_CANCEL_PARTY:
			_team.MsgLeaderCancelParty(msg.source,msg.param);
			return 0;

		case GM_MSG_MEMBER_NOT_IN_TEAM:
			_team.MsgNotifyMemberLeave(msg.source,_parent->ID,0);
			return 0;

		case GM_MSG_LEADER_KICK_MEMBER:
			_team.MsgLeaderKickMember(msg.source,XID(GM_TYPE_PLAYER,msg.param));
			return 0;

		case GM_MSG_MEMBER_LEAVE:
			_team.MsgNotifyMemberLeave(msg.source,XID(GM_TYPE_PLAYER,msg.param));
			return 0;
/*	�����Ѿ�û��ʹ����
		case GM_MSG_GET_MEMBER_POS:
			_runner->teammate_get_pos(msg.source,*(int*)(msg.content), msg.param);
			return 0;
 */

		case GM_MSG_QUERY_PLAYER_EQUIPMENT:
			{
				float ox = msg.pos.x - _parent->pos.x;
				float oz = msg.pos.z - _parent->pos.z;
				if(ox*ox + oz*oz <= GET_EQUIP_INFO_DIS*GET_EQUIP_INFO_DIS)
				{
					_runner->send_equipment_info(msg.source,*(int*)(msg.content), msg.param);
				}
			}
			return 0;
		case  GM_MSG_TEAM_PICKUP:
			{
				_runner->team_member_pickup(msg.source,msg.param,*(int*)msg.content);
			}
			return 0;
		case  GM_MSG_SERVICE_GREETING:
			_provider.id = msg.source;
			_provider.pos = msg.pos;
			_provider.id_mafia = msg.param;
			_runner->npc_greeting(msg.source);
			return 0;

		case  GM_MSG_SERVICE_DATA:
		{
			ASSERT(!_parent->IsZombie() && "��������״̬�����κη���");
			int service_type = msg.param;
			service_executor *executor = service_manager::GetExecutor(service_type);
			if(executor)
			{
				executor->Serve(this,msg.source,msg.pos,msg.content,msg.content_length);
			}
		}
		return 0;
		
		case  GM_MSG_NPC_BE_KILLED:
		{
			//�����������ٵ����� 
			ASSERT(msg.content_length == sizeof(int));
			int level = *(int*)msg.content;
			if(_invader_state && level >= _basic.level)
			{
				_invade_ctrl.ReducePariah(PARIAH_TIME_REDUCE);
			}

			//�õ���ɱ��npc����Ϣ
			if(_task_mask & TASK_MASK_KILL_MONSTER)
			{
				__PRINTF("ɱ���˹���%d\b",msg.param);
				//��������ϵͳ�Ĳ���
				PlayerTaskInterface  task_if(this);
				OnTaskKillMonster(&task_if,msg.param, level,abase::RandUniform(),0,0);
			}

			//���������ʱ��,������Ҳ������
			if(_profit_level != PROFIT_LEVEL_NONE)
			{
				_petman.KillMob(this,level);
			}
		}
		return 0;

		case GM_MSG_PLAYER_TASK_TRANSFER:
		{
			__PRINTF("�ֵ�������Ҵ�����������Ϣ\n");
			PlayerTaskInterface  task_if(this);
			OnPlayerNotify(&task_if,msg.source.id,msg.content,msg.content_length);
		}
		return 0;

		case GM_MSG_PLAYER_BECOME_INVADER:
		{
			__PRINTF("%d��ɷ����� \n",_parent->ID.id);
			if(!_nonpenalty_pvp_state && !world_manager::GetWorldFlag().nonpenalty_pvp_flag) _invade_ctrl.BecomeInvader(msg.source,msg.param);
		}
		return 0;

		case GM_MSG_PLAYER_BECOME_PARIAH:
		{
			__PRINTF("%d��ɺ����� \n",_parent->ID.id);
			if(!_nonpenalty_pvp_state && !world_manager::GetWorldFlag().nonpenalty_pvp_flag) _invade_ctrl.BecomePariah();
		}
		return 0;

		case GM_MSG_QUERY_SELECT_TARGET:
		{
			SendTo<0>(GM_MSG_NOTIFY_SELECT_TARGET,msg.source,
					((gplayer_controller*)_commander)->GetCurTarget().id);
		}
		return 0;
		
		case GM_MSG_NOTIFY_SELECT_TARGET:
		{
			((gplayer_controller*)_commander)->SelectTarget(msg.param);
		}
		return 0;

		case GM_MSG_NOTIFY_SELECT_SUBTARGET:
		{
			((gplayer_controller*)_commander)->SelectSubTarget(msg.source,msg.param);
		}
		return 0;

		case GM_MSG_SUBSCIBE_CONFIRM:
		{
			((gplayer_controller*)_commander)->SubscibeConfirm(msg.source);
		}
		return 0;

		case GM_MSG_SUBSCIBE_SUBTARGET_CONFIRM:
		{
			((gplayer_controller*)_commander)->SecondSubscibeConfirm(msg.source);
		}
		return 0;

		case GM_MSG_GATHER_REPLY:
		{
			//�ܵ����Կ��ɵ�֪ͨ
			if(HasSession())
			{
				//ȡ���ɼ�
				SendTo<0>(GM_MSG_GATHER_CANCEL,msg.source,0);
			}
			else
			{
				//��ʼ�ɼ�
				session_gather *pSession = new session_gather(this);
				bool can_be_interrupted = true;
				int eliminate_tool = -1;
				unsigned short gather_time_min=0,gather_time_max=0;
				if(msg.content_length == sizeof(gather_reply))
				{
					gather_reply * pr = (gather_reply*)msg.content;
					can_be_interrupted = pr->can_be_interrupted;
					eliminate_tool = pr->eliminate_tool;
					gather_time_min = pr->gather_time_min;
					gather_time_max = pr->gather_time_max;
				}
				if(eliminate_tool != -1)
				{
					//ֻ����Ҫɾ����Ʒ��������Ʒ��
					pSession->LockInventory();
				}
				int use_time_min = gather_time_min & 0xFF;
				int use_time_max = gather_time_max & 0xFF;
				int use_time = abase::Rand(use_time_min, use_time_max);
				pSession->SetTarget(msg.source.id,use_time,can_be_interrupted);
				AddSession(pSession);
				StartSession();
			}
		}
		return 0;

		case GM_MSG_GATHER_RESULT:
		{
			//�յ����ɽ��
			ASSERT(msg.content_length == sizeof(gather_result));
			gather_result * res = (gather_result*)msg.content;
			if(res->eliminate_tool > 0)
			{
				//�����Ʒ�Ƿ���ڣ�������ڲ��ܽ��д˲�����������Ʒ�Ļ���ɾ������Ʒ
				int rst = _inventory.Find(0,res->eliminate_tool);
				if(rst >= 0)
				{
					item& it = _inventory[rst];
					UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
					
					_inventory.DecAmount(rst,1);
					_runner->player_drop_item(gplayer_imp::IL_INVENTORY,rst,
							res->eliminate_tool,1,S2C::DROP_TYPE_USE);
				}
				else
				{
					//�޷��ɼ�
					_runner->error_message(S2C::ERR_MINE_GATHER_FAILED);
					return 0;
				}
			}

			int item_id = msg.param;
			size_t count = res->amount;
			int task_id = res->task_id;
			int life = res->life;
			int mine_type = res->mine_type;
			if(item_id > 0 && count > 0)
			{
				
				element_data::item_tag_t tag = {element_data::IMT_NULL,0};
				item_data * data = world_manager::GetDataMan().generate_item_for_drop(item_id,&tag,sizeof(tag));
				if(data)
				{
					//FirstAcquireItem(data);
					if(count > data->pile_limit) count = data->pile_limit;
					GLog::log(GLOG_INFO,"�û�%d�ɼ��õ�%d��%d",_parent->ID.id, count,item_id);

					world_manager::TestCashItemGenerated(data->type, count);
					
					data->count = count;
					if (life)
						data->expire_date = g_timer.get_systime() + life; 
					int rst = _inventory.Push(*data);
                    FirstAcquireItem(data);
					if(rst >=0) _runner->obtain_item(item_id,data->expire_date,count - data->count,_inventory[rst].count, 0,rst);
					if(data->count)
					{
						_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
						//ʣ������Ʒ�������Ԫ����䣬ֻ����30�룬��owner_timeͬ��
						int matter_life = 0;
						if (gmatter_mine_imp::MINE_TYPE_SOUL == mine_type)
							matter_life = MATTER_ITEM_SOUL_LIFE;
						DropItemData(_plane,_parent->pos,data,_parent->ID,0,0,matter_life);
						//�����������Ҫ�ͷ��ڴ�
						//���������ж�
					}
					else
					{
						FreeItem(data);
					}
				}
				else
				{
					//��Ʒ����ʧ��
					_runner->error_message(S2C::ERR_MINE_GATHER_FAILED);
				}
			}

			if(task_id > 0)
			{
				//����󣬴��ݸ�����ϵͳ
				PlayerTaskInterface  task_if(this);
				OnTaskMining(&task_if,task_id);
			}

			if(gmatter_mine_imp::MINE_TYPE_SOUL == mine_type)
			{
				_player_fatering.AddGainTimes();
			}

			world_manager::GetInstance()->OnMineGathered(_plane, res->mine_tid, GetParent());
		}
		return 0;

		case GM_MSG_TEAM_APPLY_PARTY:
		{
			_team.ApplyParty(msg.source);
		}
		return 0;
		
		case GM_MSG_TEAM_CHANGE_TO_LEADER:
		{
			_team.ChangeToLeader(msg.source);
		}
		return 0;

		case GM_MSG_TEAM_LEADER_CHANGED:
		{
			_team.LeaderChanged(msg.source);
		}
		return 0;

		case GM_MSG_TEAM_APPLY_REPLY:
		{
			_team.ApplyPartyReply(msg.source,msg.param);
		}
		return 0;

		case GM_MSG_CON_EMOTE_REQUEST:
		{
			//�յ����ض����������󣬷��͸��ͻ���
			_runner->concurrent_emote_request(msg.source.id, msg.param & 0xFFFF);
		}
		return 0;

		case GM_MSG_CON_EMOTE_REPLY:
		{
			if(msg.param & 0xFFFF) 
			{
				if(_con_emote_target == msg.source.id && ((msg.param >> 16)&0xFFFF) == _con_emote_id)
				{
					_runner->do_concurrent_emote(_con_emote_target,_con_emote_id);
					ClearConcurrentEmote();
				}
			}
			else
			{
				if(_con_emote_target == msg.source.id)
				{
					_runner->error_message(S2C::ERR_CONCURRENT_EMOTE_REFUSED);
					ClearConcurrentEmote();
				}
			}
		}
		return 0;

		case GM_MSG_GM_RECALL:
		{
			LongJump(msg.pos,msg.param);
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_MQUERY_MOVE_POS_REPLY:
		{
			ASSERT(sizeof(instance_key) == msg.content_length);
			//����򵥵�long jump��������������
			GLog::log(GLOG_INFO,"GM:%d�ƶ������%d��,tag:%d,(%f,%f,%f)",_parent->ID.id,msg.source.id,msg.param,msg.pos.x,msg.pos.y,msg.pos.z);
			if(msg.param == world_manager::GetWorldTag())
			{
				//ͬһ����������ת����Σ�
				//����������Ҳûɶ����,�������Լ���Ŀ�겻��һ��world��ʱ������Ŀ��
				/*if(!world_manager::GetInstance()->IsUniqueWorld())
				{
					_runner->error_message(S2C::ERR_CAN_NOT_JUMP_BETWEEN_INSTANCE);
				}
				else*/
				{
					LongJump(msg.pos,msg.param);
				}
				return 0;
			}

			//ִ�и�����ת�߼�
			instance_key key = *(instance_key*)msg.content;
			key.special_mask = IKSM_GM;

			//��Player���и������� 
			if(world_manager::GetInstance()->PlaneSwitch(this ,msg.pos,msg.param,key,0) < 0)
			{
				_runner->error_message(S2C::ERR_CANNOT_ENTER_INSTANCE);
			}
		}
		return 0;

		case GM_MSG_GM_MQUERY_MOVE_POS:
		{	
			int world_tag = world_manager::GetWorldTag();
			instance_key key;
			GetInstanceKey(world_tag,key);
			key.target = key.essence;
			if(world_manager::GetInstance()->IsBattleWorld())
			{
				key.target.key_level4 = _plane->w_ins_key.key1;
			}			
			else if(world_manager::GetInstance()->IsFactionWorld())
			{
				key.target.key_level3 = _plane->w_ins_key.key1;
			}
			else if(world_manager::GetInstance()->IsCountryBattleWorld())
			{
				key.target.key_level4 = _plane->w_ins_key.key1;
			}
			else if(world_manager::GetInstance()->GetWorldType() == WORLD_TYPE_PARALLEL_WORLD)
			{
				key.target.key_level4 = _plane->w_ins_key.key1;
			}
			else if(world_manager::GetInstance()->GetWorldType() == WORLD_TYPE_TRICKBATTLE)
			{
				key.target.key_level4 = _plane->w_ins_key.key1;
			}
			SendTo<0>(GM_MSG_GM_MQUERY_MOVE_POS_REPLY,msg.source,world_manager::GetWorldTag(),&key,sizeof(key));
		}
		return 0;
		
		case GM_MSG_GM_CHANGE_EXP:
		{
			return 0;	//��ʱ��ֹ��
			int exp = msg.param;
			int sp = *(int*)msg.content;
			if(exp <0 ) exp = 0;
			if(sp < 0) sp = 0;
			if(exp > 100000000) exp = 100000000;
			if(sp > 100000000) sp = 100000000;
			ReceiveExp(exp,sp);
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_ENDUE_ITEM:
		{
			return 0;	//��ʱ��ֹ��
			if(_inventory.GetEmptySlotCount() <=0)
			{
				//���ͻ�����Ϣ....
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_INVENTORY_IS_FULL);
				return 0;
			}
			int item_id = msg.param;
			size_t count  = *(int*)msg.content;
			element_data::item_tag_t tag = {element_data::IMT_NULL,0};
			item_data * data = world_manager::GetDataMan().generate_item_for_shop(item_id,&tag,sizeof(tag));
			if(!data) 
			{
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_PRODUCE_FAILED);
				return 0;
			}
			if(count > data->pile_limit) count = data->pile_limit;
			data->count = count;
			if(ObtainItem(gplayer_imp::IL_INVENTORY,data)) FreeItem(data);
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_RESET_PP:
		{
			_basic.status_point += player_template::Rollback(GetPlayerClass(),_base_prop);
			//ʹ��װ����Ч
			RefreshEquipment();
			//����װ�������ݣ������˿���
			CalcEquipmentInfo();

			//���Լ���Ϣ
			PlayerGetProperty();
		}
		return 0;

		case GM_MSG_GM_ENDUE_SELL_ITEM:
		{
			return 0;	//��ʱ��ֹ��
			if(_inventory.GetEmptySlotCount() <=0)
			{
				//���ͻ�����Ϣ....
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_INVENTORY_IS_FULL);
				return 0;
			}
			int item_id = msg.param;
			size_t count  = *(int*)msg.content;
			const void *pBuf = world_manager::GetDataMan().get_item_for_sell(item_id);
			if(!pBuf) 
			{
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_PRODUCE_FAILED);
				return 0;
			}
			item_data * data = DupeItem(*(const item_data*)pBuf);
			if(count > data->pile_limit) count = data->pile_limit;
			data->count = count;
			if(ObtainItem(gplayer_imp::IL_INVENTORY,data)) FreeItem(data);
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_REMOVE_ITEM:
		{
			int item_id = msg.param;
			size_t num = *(int*)msg.content;
			if(item_id < 0) 
			{
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_ITEM_NOT_IN_INVENTORY);
				return 0;
			}
			int rst = 0;
			while(num && (rst = _inventory.Find(rst,item_id)) >= 0)
			{
				size_t count = num;
				if(_inventory[rst].count < count) count = _inventory[rst].count;
				_inventory.DecAmount(rst,count);
				_runner->player_drop_item(gplayer_imp::IL_INVENTORY,rst,item_id,count,S2C::DROP_TYPE_GM);
				num -= count;
				rst ++;
			}
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_ENDUE_MONEY:
		{
			return 0;	//��ʱ��ֹ��
		}
		return 0;

		case GM_MSG_GM_OFFLINE:
		{
			LostConnection(gplayer_imp::PLAYER_OFF_LPG_DISCONNECT);
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_DEBUG_COMMAND:
		{
			gplayer_controller * pCtrl = ((gplayer_controller*)_commander);
			pCtrl->SetDebugMode(!pCtrl->GetDebugMode());
			SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,0);
		}
		return 0;

		case GM_MSG_GM_QUERY_SPEC_ITEM:
		{
			ASSERT(msg.content_length == sizeof(msg_query_spec_item_t));
			msg_query_spec_item_t & data = *(msg_query_spec_item_t*)msg.content;
			raw_wrapper ar;
			FindSpecItem(IL_INVENTORY, data.type, ar);
			FindSpecItem(IL_EQUIPMENT, data.type, ar);
			FindSpecItem(IL_TASK_INVENTORY, data.type, ar);
			FindSpecItem(IL_TRASH_BOX, data.type, ar);
			FindSpecItem(IL_TRASH_BOX2, data.type, ar);
			FindSpecItem(IL_TRASH_BOX3, data.type, ar);
			FindSpecItem(IL_TRASH_BOX4, data.type, ar);
			FindSpecItem(IL_USER_TRASH_BOX, data.type, ar);
			_runner->send_spec_item_list(data.cs_index, data.cs_sid, msg.source.id, data.type, ar.data(), ar.size());
		}
		return 0;

		case GM_MSG_GM_REMOVE_SPEC_ITEM:
		{
			ASSERT(msg.content_length == sizeof(msg_remove_spec_item_t));
			msg_remove_spec_item_t & data = *(msg_remove_spec_item_t*)msg.content;
			int ret = RemoveSpecItem(data.where, data.index, data.count, data.type);
			_runner->send_error_message(data.cs_index, data.cs_sid, msg.source.id, ret);
		}
		return 0;

		case GM_MSG_PLANE_SWITCH_REPLY:
		{
			//����Ϣֻ��NORMAL״̬�¿��Խ��д���
			if(_player_state != PLAYER_STATE_NORMAL 
					&& _player_state != PLAYER_STATE_BIND
					&& _player_state != PLAYER_SIT_DOWN
					&& _player_state != PLAYER_STATE_MARKET)
			{
				return 0;
			}
			LeaveAbnormalState();

			if(msg.source.id == world_manager::GetWorldIndex())
			{
				//������Ϣת�����Ʋ������ظ�ת����Ϣ�����账��
				return 0;
			}
			ASSERT(msg.content_length == sizeof(instance_key));
			instance_key * key = (instance_key*)msg.content;
			SwitchSvr(msg.source.id,_parent->pos,msg.pos,key);
		}
		return 0;

		case GM_MSG_LEAVE_COSMETIC_MODE:
		{	
			if(_player_state == PLAYER_STATE_COSMETIC) LeaveCosmeticMode(msg.param); //��ʱ״̬���ܲ���
		}
		return 0;

		case GM_MSG_DBSAVE_ERROR:
		{
			_db_save_error ++;
			if(_db_save_error >= 3)
			{
				GLog::log(GLOG_ERR,"�û�%d���ڴ��̴���������࣬��ǿ��ִ�������߼�",_parent->ID.id);
				LostConnection(PLAYER_OFF_LPG_DISCONNECT);
			}
		}
		return 0;

		case GM_MSG_HURT:
		{
			if(!_parent->IsZombie()) 
			{
				ASSERT(msg.content_length == sizeof(msg_hurt_extra_info_t));
				msg_hurt_extra_info_t & data = *(msg_hurt_extra_info_t*)msg.content;
				int damage = msg.param;
				if(damage > _basic.hp)
				{
					if(!msg.source.IsPlayerClass())
					{
						//���������ҹ����������Ļ�����ô�ж϶Է��Ƿ�����
						//����Ѿ������������������
						world::object_info info;
						enum
						{
							ALIVE = world::QUERY_OBJECT_STATE_ACTIVE | world::QUERY_OBJECT_STATE_ZOMBIE
						};
						if(!_plane->QueryObject(msg.source,info)  ||
							((info.state & ALIVE) != world::QUERY_OBJECT_STATE_ACTIVE))
						{
							//���������������������������
							__PRINTF("�˺����뷢��\n");
							damage >>= 1;
						}
					}
				}

				DoDamage(damage);
				if(_basic.hp == 0)
				{
					Die(msg.source,data.orange_name && _invader_state == INVADER_LVL_0,data.attacker_mode,0);
				}
			}
		}
		return 0;

		case GM_MSG_ENABLE_PVP_DURATION:
		{
			__PRINTF("%d����PK״̬\n",_parent->ID.id);
			SetPVPCombatState();
		}
		return 0;

		case GM_MSG_PLAYER_DUEL_REQUEST:
		{
			//������ 
			if(_parent->pos.squared_distance(msg.pos) <= 400)
			{
				_duel.MsgDuelRequest(this,msg.source);
			}
			else
			{
				SendTo<0>(GM_MSG_PLAYER_DUEL_REPLY,msg.source,player_duel::DUEL_REPLY_OUT_OF_RANGE);
			}
		}
		return 0;

		case GM_MSG_PLAYER_DUEL_REPLY:
		{
			if(_parent->pos.squared_distance(msg.pos) <= 400)
			{
				_duel.MsgDuelReply(this,msg.source,msg.param);
			}
			else
			{
				SendTo<0>(GM_MSG_PLAYER_DUEL_CANCEL,msg.source,0);
			}
		}
		return 0;

		case GM_MSG_PLAYER_DUEL_PREPARE:
		{
			_duel.MsgDuelPrepare(this,msg.source);
		}
		return 0;

		case GM_MSG_PLAYER_DUEL_START:
		{
			if(msg.source != _parent->ID)
			{
				_duel.MsgDuelStart(this,msg.source);
			}
			else
			{
				ASSERT(msg.content_length == sizeof(XID));
				_duel.MsgDuelStart(this,*(XID *)msg.content);
			}
		}
		return 0;

		case GM_MSG_PLAYER_DUEL_CANCEL:
		{
			_duel.MsgDuelCancel(this,msg.source);
		}
		return 0;

		case GM_MSG_PLAYER_DUEL_STOP:
		{
			if(msg.source != _parent->ID)
			{
				_duel.MsgDuelStop(this,msg.source,msg.param);
			}
			else
			{
				ASSERT(msg.content_length == sizeof(XID));
				_duel.MsgDuelStop(this,*(XID *)msg.content,msg.param);
			}
		}
		return 0;

		case GM_MSG_PLAYER_BIND_REQUEST:
		{
			if(_parent->pos.squared_distance(msg.pos) <= 100)
			{
				_bind_player.MsgRequest(this,msg.source);
			}
		}
		return 0;
		
		case GM_MSG_PLAYER_BIND_INVITE:
		{
			if(_parent->pos.squared_distance(msg.pos) <= 100)
			{
				_bind_player.MsgInvite(this,msg.source);
			}
		}
		return 0;
		
		case GM_MSG_PLAYER_BIND_REQUEST_REPLY:
		{
			if(_parent->pos.squared_distance(msg.pos) <= 100)
			{
				_bind_player.MsgRequestReply(this,msg.source,msg.param);
			}
		}
		return 0;
		
		case GM_MSG_PLAYER_BIND_INVITE_REPLY:
		{
			if(_parent->pos.squared_distance(msg.pos) <= 100)
			{
				_bind_player.MsgInviteReply(this,msg.source,msg.param);
			}
		}
		return 0;
		
		case GM_MSG_PLAYER_BIND_PREPARE:
		{
			_bind_player.MsgPrepare(this,msg.source);
		}
		return 0;

		case GM_MSG_PLAYER_BIND_LINK:
		{
			_bind_player.MsgBeLinked(this,msg.source,msg.pos);
		}
		return 0;
		
		case GM_MSG_PLAYER_BIND_STOP:
		{
			_bind_player.MsgStopLinked(this,msg.source);
		}
		return 0;
		
		case GM_MSG_PLAYER_BIND_FOLLOW:
		{
			//50���ڲ���follow
			if(_parent->pos.squared_distance(msg.pos) <= 50.f*50.f)
			{
				_bind_player.MsgFollowOther(this,msg.source,msg.pos);
			}
		}
		return 0;

		case GM_MSG_QUERY_EQUIP_DETAIL:
		{
			if(GetEnemyFaction() & msg.param)
			{
				SendTo<0>(GM_MSG_ERROR_MESSAGE,msg.source,S2C::ERR_CANNOT_QUERY_ENEMY_EQUIP);
				return 0;
			}
			float dis = msg.pos.squared_distance(_parent->pos);
			if(dis <= 30*30)
			{
				ASSERT(msg.content_length == sizeof(int)*2);
				int * tmp = (int*)(msg.content);
				int cs_index = tmp[0];
				int cs_sid = tmp[1];

				static const  int query_list[] = {
					item::EQUIP_INDEX_WEAPON,
					item::EQUIP_INDEX_HEAD,	
					item::EQUIP_INDEX_NECK,
					item::EQUIP_INDEX_SHOULDER,
					item::EQUIP_INDEX_BODY,	
					item::EQUIP_INDEX_WAIST,
					item::EQUIP_INDEX_LEG,
					item::EQUIP_INDEX_FOOT,
					item::EQUIP_INDEX_WRIST,
					item::EQUIP_INDEX_FINGER1,
					item::EQUIP_INDEX_FINGER2,
					item::EQUIP_INDEX_FLYSWORD,
					item::EQUIP_INDEX_FASHION_BODY,
					item::EQUIP_INDEX_FASHION_LEG,
					item::EQUIP_INDEX_FASHION_FOOT,
					item::EQUIP_INDEX_FASHION_WRIST,
					item::EQUIP_INDEX_BIBLE,
					item::EQUIP_INDEX_BUGLE,
					item::EQUIP_INDEX_TWEAK,
					item::EQUIP_INDEX_ELF,
					item::EQUIP_INDEX_FASHION_HEAD,
					item::EQUIP_INDEX_FASHION_WEAPON,
					item::EQUIP_INDEX_GENERALCARD1,
					item::EQUIP_INDEX_GENERALCARD2,
					item::EQUIP_INDEX_GENERALCARD3,
					item::EQUIP_INDEX_GENERALCARD4,
					item::EQUIP_INDEX_GENERALCARD5,
					item::EQUIP_INDEX_GENERALCARD6,
				};

				raw_wrapper rw(1024);
				_equipment.DetailSavePartial(rw,query_list,sizeof(query_list)/sizeof(int));
				_runner->send_equip_detail(cs_index, cs_sid,msg.source.id,rw.data(),rw.size());
			}
		}
		return 0;

		case GM_MSG_PLAYER_RECALL_PET:
		{
			_petman.RecallPet(this);
		}
		return 0;
		
		case GM_MSG_REMOVE_ITEM:
		{
			int item_id = msg.param;
			if(item_id <= 0) 
			{
				return 0;
			}
			int rst = 0;
			rst = _inventory.Find(rst,item_id);
			if(rst >= 0)
			{
				item& it = _inventory[rst];
				UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
				
				_inventory.DecAmount(rst,1);
				_runner->player_drop_item(gplayer_imp::IL_INVENTORY,rst,item_id,1,S2C::DROP_TYPE_GM);
			}
		}
		return 0;

		case GM_MSG_PET_RELOCATE_POS:
		{
			int dis = *(int*)msg.content;
			if(!_petman.RelocatePos(this,msg.source, msg.param,dis))
			{
				SendTo<0>(GM_MSG_PET_DISAPPEAR,msg.source,msg.param);
			}
		}
		return 0;

		case GM_MSG_PET_NOTIFY_HP:
		{
			if(msg.content_length == sizeof(msg_pet_hp_notify))
			{
				msg_pet_hp_notify * pInfo = (msg_pet_hp_notify *) msg.content;
				if(!_petman.NotifyPetHP(this, msg.source, msg.param,*pInfo))
				{
					SendTo<0>(GM_MSG_PET_DISAPPEAR,msg.source,msg.param);
				}
			}
		}
		return 0;

		case GM_MSG_PET_NOTIFY_DEATH:
		{
			_petman.PetDeath(this, msg.source, msg.param);
		}
		return 0;

		case GM_MSG_PET_SET_COOLDOWN:
		{
			if(msg.content_length == sizeof(int))
			{
				_petman.PetSetCoolDown(this, msg.source, msg.param,*(int*)msg.content);
			}
		}
		return 0;

		case GM_MSG_PET_ANTI_CHEAT:
		{
			OnAntiCheatAttack(0.02f);
		}
		return 0;

		case GM_MSG_MOB_BE_TRAINED:
		{
			if(_inventory.GetEmptySlotCount() > 0)
			{
				OnAntiCheatAttack(0.10f);
				int id = msg.param;
				const void *pBuf = world_manager::GetDataMan().get_item_for_sell(id);
				if(!pBuf) 
				{
					return 0;
				}
				item it;
				if(MakeItemEntry(it, *(const item_data *)pBuf))
				{
					//�Ƿ���Ҫ �������ó��ﵰ�Ŀɼ�ID $$$$$$$		
					int rst = _inventory.PushInEmpty(0,it);
					if(rst >=0)
					{
						_runner->obtain_item(id,it.expire_date,1,_inventory[rst].count, IL_INVENTORY,rst);
					}
					it.Release();
				}
			}
			else
			{
				_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			}
		}
		return 0;

		case GM_MSG_QUERY_PROPERTY:
		{
			struct S2C::CMD::player_property::_data data;
			memset(&data,0,sizeof(data));
			ASSERT(MAGIC_CLASS == 5);
			data.id					= _parent->ID.id;
			data.hp 				= _basic.hp;
			data.mp 				= _basic.mp;
			data.damage_low 		= _cur_prop.damage_low;
			data.damage_high		= _cur_prop.damage_high;
			data.damage_magic_low 	= _cur_prop.damage_magic_low;
			data.damage_magic_high 	= _cur_prop.damage_magic_high;
			data.defense 			= _cur_prop.defense;		
			data.resistance[0] 		= _cur_prop.resistance[0];
			data.resistance[1] 		= _cur_prop.resistance[1];
			data.resistance[2] 		= _cur_prop.resistance[2];
			data.resistance[3] 		= _cur_prop.resistance[3];
			data.resistance[4] 		= _cur_prop.resistance[4];
			data.attack 			= _cur_prop.attack;	
			data.armor 				= _cur_prop.armor;	
			data.attack_speed 		= _cur_prop.attack_speed;
			data.run_speed 			= _cur_prop.run_speed;	
			data.attack_degree		= _attack_degree;
			data.defend_degree		= _defend_degree;
			data.crit_rate			= _crit_rate+_base_crit_rate;
			if(data.crit_rate > 100)	data.crit_rate = 100;
			data.damage_reduce		= _damage_reduce;
			if(data.damage_reduce > 75)	data.damage_reduce = 75;
			data.prayspeed			= _skill.GetPraySpeed();
			if(data.prayspeed >= 100) data.prayspeed = 90;	//����֣��������Ǳ߾�������д��
			data.crit_damage_bonus 	= _crit_damage_bonus;
			data.invisible_degree 	= ((gplayer*)_parent)->invisible_degree;
			data.anti_invisible_degree = ((gplayer*)_parent)->anti_invisible_degree;
			data.vigour = GetVigour();
			SendTo<0>(GM_MSG_QUERY_PROPERTY_REPLY,msg.source,msg.param,&data,sizeof(data));	
		}
		return 0;

		case GM_MSG_QUERY_PROPERTY_REPLY:
		{
			if(msg.param >=0)
			{
				//����Ʒ����ͻ��˷�������
				ASSERT(msg.param >=0 && (size_t)msg.param < _inventory.Size());
				item & it = _inventory[msg.param];
				if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_QUERYOTHERPROPERTY)
					return 0;
				UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
				_runner->player_drop_item(IL_INVENTORY, msg.param, it.type, 1, S2C::DROP_TYPE_USE);
				_inventory.DecAmount(msg.param, 1);	
			}
			
			struct S2C::CMD::player_property::_self_data self_data;
			memset(&self_data,0,sizeof(self_data));
			self_data.damage_reduce = _damage_reduce;
			if(self_data.damage_reduce > 75) self_data.damage_reduce = 75;
			self_data.prayspeed		= _skill.GetPraySpeed();
			if(self_data.prayspeed >= 100) self_data.prayspeed = 90;	//����֣��������Ǳ߾�������д��
			_runner->send_others_property(msg.content, msg.content_length, &self_data, sizeof(self_data));
		}
		return 0;
		
		case GM_MSG_NOTIFY_CLEAR_INVISIBLE:
		{
			//�յ�����ȡ����������Ϣ
			if(msg.source == _petman.GetCurPet())
			{
				if(((gplayer*)_parent)->IsInvisible())
					_filters.RemoveFilter(FILTER_INDEX_INVISIBLE);
			}			
		}
		return 0;
		
		case GM_MSG_CONTRIBUTION_TO_KILL_NPC:
		{
			ASSERT(msg.content_length == sizeof(msg_contribution_t));
			if(_task_mask & TASK_MASK_KILL_PQ_MONSTER)
			{
				if(msg.param == world_manager::GetWorldTag() 
					&& msg.pos.squared_distance(_parent->pos) <= (NORMAL_EXP_DISTANCE * NORMAL_EXP_DISTANCE))
				{
					msg_contribution_t & contri = *(msg_contribution_t*)(msg.content);
					//��������ϵͳ�Ĳ���
					PlayerTaskInterface  task_if(this);
					OnTaskKillPQMonster(&task_if,contri.npc_id,contri.is_owner,contri.team_contribution,contri.team_member_count, contri.personal_contribution);
				}
			}
		}
		return 0;
		
		case GM_MSG_GROUP_CONTRIBUTION_TO_KILL_NPC:
		{
			msg_group_contribution_t& contri = *(msg_group_contribution_t*)msg.content;
			ASSERT(msg.content_length == sizeof(msg_group_contribution_t)+contri.count*sizeof(msg_group_contribution_t::_list));
			if(_team.IsLeader())
			{
				XID mlist[TEAM_MEMBER_CAPACITY];
				float mcontri[TEAM_MEMBER_CAPACITY];
				int mlist_count = 0;
				char flag_list[TEAM_MEMBER_CAPACITY];
				memset(flag_list,0,sizeof(flag_list));
				float total_contribution = 0.f;

				for(int i=0; i<contri.count; i++)
				{
					int index;
					A3DVECTOR pos;
					int level;
					int tag;
					int plane_index;
					if((index = _team.GetMember(contri.list[i].xid, pos, level, tag, plane_index)) >= 0)
					{
						flag_list[index] = 1;
						if(tag == msg.param && pos.squared_distance(msg.pos) <= TEAM_EXP_DISTANCE*TEAM_EXP_DISTANCE)				
						{
							mlist[mlist_count] = contri.list[i].xid;
							mcontri[mlist_count] = contri.list[i].contribution;
							mlist_count ++;
							total_contribution += contri.list[i].contribution;
						}
					}
					else
					{
						msg_contribution_t data;
						data.npc_id = contri.npc_id;
						data.is_owner = false;
						data.team_contribution = contri.list[i].contribution;
						data.team_member_count = 1;
						data.personal_contribution = contri.list[i].contribution;
						MSG new_msg;
						BuildMessage(new_msg, GM_MSG_CONTRIBUTION_TO_KILL_NPC, contri.list[i].xid, msg.source, msg.pos,
							msg.param, &data, sizeof(data));
						_plane->PostLazyMessage(new_msg);
					}
				}

				int member_count = _team.GetMemberNum();
				for(int i=0; i<member_count; i++)
				{
					if(flag_list[i]) continue;
					const player_team::member_entry &ent = _team.GetMember(i);			
					if(ent.data.world_tag == msg.param 
						&& ent.pos.squared_distance(msg.pos) <= TEAM_EXP_DISTANCE*TEAM_EXP_DISTANCE)
					{
						mlist[mlist_count] = ent.id;	
						mcontri[mlist_count] = 0;
						mlist_count ++;
					}
				}
			
				if(mlist_count)
				{
					msg_contribution_t data; 
					data.npc_id = contri.npc_id;
					data.is_owner = contri.is_owner;
					data.team_contribution = total_contribution;
					data.team_member_count = mlist_count;
					MSG new_msg;
					BuildMessage(new_msg, GM_MSG_CONTRIBUTION_TO_KILL_NPC, XID(-1,-1), msg.source, msg.pos,
						msg.param, &data, sizeof(data));
					for(int i=0; i<mlist_count; i++)
					{
						new_msg.target = mlist[i];
						data.personal_contribution = mcontri[i];
						_parent->plane->PostLazyMessage(new_msg);
					}
				}
			}
		}
		return 0;

		case GM_MSG_REBUILD_TEAM_INSTANCE_KEY_REQ:
		{
			ASSERT(msg.content_length == sizeof(instance_hash_key));
			instance_hash_key * ikey = (instance_hash_key *)msg.content;
			if(_team.IsLeader() && 
					_team_ins_key_list[msg.param].first == ikey->key1 && _team_ins_key_list[msg.param].second == ikey->key2)
				ResetInstance(msg.param);
		}
		return 0;

		case GM_MSG_REBUILD_TEAM_INSTANCE_KEY:
		{
			ASSERT(msg.content_length == 2*sizeof(instance_hash_key));
			instance_hash_key * ikey = (instance_hash_key *)msg.content;
			if(_team.IsInTeam() && !_team.IsLeader() && 
					_team_ins_key_list[msg.param].first == ikey->key1 && _team_ins_key_list[msg.param].second == ikey->key2)
			{
				ikey ++;
				_team_ins_key_list[msg.param].first = ikey->key1;
				_team_ins_key_list[msg.param].second = ikey->key2;
			}
		}
		return 0;

		case GM_MSG_PLANT_PET_NOTIFY_DEATH:
		{
			_plantpetman.PlantDeath(this, msg.source, msg.param);
		}
		return 0;

		case GM_MSG_PLANT_PET_NOTIFY_HP:
		{
			if(msg.content_length == sizeof(msg_plant_pet_hp_notify))
			{
				msg_plant_pet_hp_notify * pInfo = (msg_plant_pet_hp_notify*)msg.content;
				if(!_plantpetman.NotifyPlantHP(this, msg.source, msg.param, *pInfo))
				{
					SendTo<0>(GM_MSG_PET_DISAPPEAR,msg.source,msg.param);
				}
			}		
		}
		return 0;

		case GM_MSG_PLANT_PET_NOTIFY_DISAPPEAR:
		{
			if(!_plantpetman.PlantDisappear(this,msg.source, msg.param))
			{
				SendTo<0>(GM_MSG_PET_DISAPPEAR,msg.source,msg.param);
			}
		}
		return 0;

		case GM_MSG_CONGREGATE_REQUEST:
		{
			ASSERT(msg.content_length == sizeof(msg_congregate_req_t));
			msg_congregate_req_t & data = *(msg_congregate_req_t *)msg.content;
			RecvCongregateRequest(msg.param, msg.source.id, data.world_tag, msg.pos, data.level_req, data.sec_level_req, data.reincarnation_times_req);
		}
		return 0;

		case GM_MSG_REJECT_CONGREGATE:
		{
			_runner->reject_congregate(msg.param, msg.source.id);
		}
		return 0;

		case GM_MSG_NPC_BE_KILLED_BY_OWNER:
		{
			//�����������ٵ����� 
			ASSERT(msg.content_length == sizeof(msg_dps_dph_t));
			msg_dps_dph_t & data = *(msg_dps_dph_t*)msg.content;
			if(_invader_state && data.level >= _basic.level)
			{
				_invade_ctrl.ReducePariah(PARIAH_TIME_REDUCE);
			}

			//�õ���ɱ��npc����Ϣ
			if(_task_mask & TASK_MASK_KILL_MONSTER)
			{
				__PRINTF("ɱ���˹���%d\b",msg.param);
				//��������ϵͳ�Ĳ���
				PlayerTaskInterface  task_if(this);
				if(data.update_rank)
					OnTaskKillMonster(&task_if, msg.param, data.level, abase::RandUniform(), data.dps, data.dph);
				else
					OnTaskKillMonster(&task_if, msg.param, data.level, abase::RandUniform(), 0, 0);
			}
			
			if(data.update_rank)
			{
				//���뵽���а���
				int cls = -1;
				bool gender = false;
				GetPlayerClass(cls, gender);
				world_manager::GetInstance()->DpsRankUpdateRankInfo(_parent->ID.id, _basic.level, cls, data.dps, data.dph);
			}
		}
		return 0;

		case GM_MSG_EXCHANGE_POS:
		{
			ASSERT(msg.content_length == sizeof(A3DVECTOR));
			if(_parent->ID == msg.source)
				Teleport(*(A3DVECTOR*)msg.content, 0, 0);
			else
				Teleport(msg.pos, 0, 0);
		}
		return 0;		

		case GM_MSG_QUERY_INVENTORY_DETAIL:
		{
			ASSERT(msg.content_length == sizeof(msg_player_t));
			msg_player_t & data = *(msg_player_t*)msg.content;
		
			raw_wrapper rw(1024);
			unsigned char size;
			_inventory.DetailSave(rw);
			size = _inventory.Size();
			_runner->send_inventory_detail(data.cs_index, data.cs_sid, data.id, GetMoney(), size, rw.data(),rw.size());
		}
		return 0;

		case GM_MSG_PLAYER_KILLED_BY_PLAYER:
		{
			ASSERT(msg.content_length == sizeof(msg_player_killed_info_t));
			msg_player_killed_info_t & data = *(msg_player_killed_info_t*)msg.content;

			if(_task_mask & TASK_MASK_KILL_PLAYER)
			{
				PlayerTaskInterface  task_if(this);
				OnTaskKillPlayer(&task_if, data.cls, data.gender, data.level, data.force_id, abase::RandUniform());
			}
		}
		return 0;

		case GM_MSG_LONGJUMP:
		{
			ASSERT(msg.content_length == sizeof(A3DVECTOR));
			A3DVECTOR & pos = *(A3DVECTOR *)msg.content;
			LongJump(pos, msg.param);
		}
		return 0;

		case GM_MSG_MAFIA_PVP_AWARD:
		{
			ASSERT(msg.content_length == sizeof(msg_mafia_pvp_award_t));
			msg_mafia_pvp_award_t& mfa = *(msg_mafia_pvp_award_t*)msg.content;
			OnMafiaPvPAward(msg.param, msg.source, msg.pos, mfa.mafia_id, mfa.domain_id);			
		}
		return 0;
		
		case GM_MSG_REDUCE_CD:
		{
			ASSERT(msg.content_length == sizeof(msg_reduce_cd_t));
			msg_reduce_cd_t & data = *(msg_reduce_cd_t *)msg.content;
			if(!CheckCoolDown(data.skill_id))
				SetCoolDown(data.skill_id,data.msec > 0 ? data.msec : 1);
		}
		return 0;
	
		case GM_MSG_DELIVER_TASK:
		{
			PlayerTaskInterface task_if(this);
			OnTaskManualTrig(&task_if, msg.param, false);
		}
		return 0;
		
		case GM_MSG_DELIVER_STORAGE_TASK:
		{
			DATA_TYPE dt;
			const TASK_LIST_CONFIG* pConfig = (TASK_LIST_CONFIG*)(world_manager::GetDataMan().get_data_ptr(msg.param, ID_SPACE_CONFIG, dt));
			if (dt != DT_TASK_LIST_CONFIG || !pConfig) return 0;
			int sizeTasks = sizeof(pConfig->id_tasks) / sizeof(pConfig->id_tasks[0]);
			int range = 0;
			for (; range < sizeTasks; ++range)
			{
				if(0 == pConfig->id_tasks[range]) break;
			}
			if(0 == range && 0 == pConfig->id_tasks[0])	return 0;
			int index = abase::Rand(0,range > 0 ? range - 1 : 0);
			
			PlayerTaskInterface task_if(this);
			OnTaskManualTrig(&task_if, pConfig->id_tasks[index], false);
		}
		return 0;
	}
	return gactive_imp::MessageHandler(pPlane,msg);
}

void 
gplayer_imp::LevelUp()
{
	int cls = GetPlayerClass();
	bool is_level_up = false;
	do
	{
		int exp = player_template::GetLvlupExp(cls,_basic.level);
		if(exp > _basic.exp)
		{	
			break;
		}

		_basic.exp -= exp;
		if(!player_template::LevelUp(cls,_basic.level,_base_prop))
		{
			GLog::log(GLOG_ERR,"�û�%d����ʱ��������,ְҵ%d,����%d",_parent->ID.id,cls,_basic.level);
			PlayerForceOffline();
			return ;
		}
		_basic.level ++;
		_basic.status_point += player_template::GetStatusPointPerLevel();
		is_level_up = true;
		_runner->level_up();
		size_t m = GetMoney() + _trashbox.GetMoney();
		int tu = GetPlayEd();
		GLog::upgrade(_parent->ID.id,_basic.level,m);
		GLog::log(GLOG_INFO,"�û�%d������%d����Ǯ%d,��Ϸʱ��%d:%02d:%02d",
				_parent->ID.id, _basic.level,m,tu/3600,(tu/60)%60,tu%60);
		if(_basic.level >= player_template::GetMaxLevel())
		{
			_basic.exp = 0;
			break;
		}
	}while(1);
	if(is_level_up)
	{
		//����Ҫ���¼�����ʷ��ߵȼ�����Ϊ�������Լ�����õ�
		_player_reincarnation.CalcHistoricalMaxLevel();

		gplayer * pPlayer = GetParent();
		int prev_invisible_degree = pPlayer->invisible_degree;
		int prev_anti_invisible_degree = pPlayer->anti_invisible_degree;
		bool invisible_changed = false;
		
		property_policy::UpdatePlayer(cls,this);
		if(!pPlayer->IsZombie())
		{
			//�����Ѫ��ħ����
			_basic.hp = _cur_prop.max_hp;
			_basic.mp = _cur_prop.max_mp;
		}

		if(!world_manager::GetWorldParam().pve_mode)
		{
			TestPKProtected();
		}
	
		if(pPlayer->IsInvisible() && pPlayer->invisible_degree > prev_invisible_degree)
		{
			//����ǰ��������״̬���������ȼ�����
			_runner->on_inc_invisible(prev_invisible_degree,pPlayer->invisible_degree);
			__PRINTF("����%d��������\n",pPlayer->invisible_degree);
			invisible_changed = true;
		}
		if(pPlayer->anti_invisible_degree > prev_anti_invisible_degree)
		{
			__PRINTF("�����ȼ�%d\n",pPlayer->anti_invisible_degree);
			_runner->on_inc_anti_invisible(prev_anti_invisible_degree,pPlayer->anti_invisible_degree);
			invisible_changed = true;
		}
		if(invisible_changed) _petman.NotifyInvisibleData(this);
		
		UpdateBaseSoulPower();
		//�ȼ������޺������޷���þ��飬��ͼ����ת������
		if(_basic.level >= player_template::GetMaxLevel()) _player_reincarnation.TryActivateTome();
		_player_reincarnation.CalcExpBonus();
		
		GMSV::SendSynMutaData(pPlayer->ID.id,_basic.level,_player_reincarnation.GetTimes());
		PlayerTaskInterface  task_if(this);
		OnTaskPlayerLevelUp(&task_if);
		_level_up = true;
	}
}

void 
gplayer_imp::ReceiveGroupExp(const A3DVECTOR &pos,int total_damage,int exp,int sp,int level,size_t count, int npcid,int npctag, int npc_planeindex, float r, const msg_grpexp_t * list)
{
	//���ȳ�ȥ�����Ѿ��뿪�Ķ�Ա
	int member_damage = 0;
	ASSERT(total_damage >0);
	float factor = 1.f/(float)(total_damage);
	char flag_list[TEAM_MEMBER_CAPACITY];
	memset(flag_list,0,sizeof(flag_list));
	int mlist[TEAM_MEMBER_CAPACITY];
	int mlist_count = 0;
	int max_level = 0;
	int min_level = 0xFFFF;
	int total_level = 0;
	for(size_t i = 0; i < count ; i ++)
	{
		int damage = list[i].damage;
		A3DVECTOR mpos;
		int level;
		int index;
		int tag;
		int plane_index;
		if((index = _team.GetMember(list[i].who,mpos,level,tag,plane_index)) >= 0)
		{
			flag_list[index] = 1;
			if(tag == npctag && plane_index == npc_planeindex && mpos.squared_distance(pos) <= TEAM_EXP_DISTANCE*TEAM_EXP_DISTANCE)
			{
				member_damage += damage;
				mlist[mlist_count] = index;
				mlist_count ++;
				if(level >= MIN_TEAM_DISEXP_LEVEL)
				{
					total_level += level;
				}
				else
				{
					total_level += MIN_TEAM_DISEXP_LEVEL;
				}

				if(level > max_level) max_level = level;
				if(level < min_level) min_level = level;

			}

		}
		else
		{
			msg_exp_t data;
			float tmp = factor * damage;
			data.level = level;
			data.exp = (int)(exp * tmp + 0.5f);
			data.sp = (int)(sp * tmp + 0.5f);
			if(data.exp > 0)
			{
				MSG msg;
				BuildMessage(msg,GM_MSG_EXPERIENCE,list[i].who,list[i].who,pos,0,&data,sizeof(data));
				_plane->PostLazyMessage(msg);
			}
		}
	}

	if(!member_damage || !mlist_count || total_level <= 0) return ;
	if(member_damage < total_damage)
	{
		factor *= member_damage;
		exp = (int)( factor * exp + 0.5f);
		sp = (int)( factor * sp + 0.5f);
	}

	//�������е�������� �м�����������Ѿ������˵�
	for(int i = 0; i < _team.GetMemberNum(); i ++)
	{
		if(flag_list[i]) continue;
		const player_team::member_entry &ent = _team.GetMember(i);
		if(ent.data.world_tag != npctag || ent.data.plane_index != npc_planeindex || ent.pos.squared_distance(pos) > TEAM_EXP_DISTANCE*TEAM_EXP_DISTANCE)
		{
			continue;
		}
		mlist[mlist_count] = i;
		mlist_count ++;
		int level = ent.data.level;
		if(level >= MIN_TEAM_DISEXP_LEVEL)
		{
			total_level += level;
		}
		else
		{
			total_level += MIN_TEAM_DISEXP_LEVEL;
		}
		if(level > max_level) max_level = level;
		if(level < min_level) min_level = level;
	}

	//_team.DispatchExp(pos,exp,sp,level);
	_team.DispatchExp(pos,mlist,mlist_count, exp,sp,level,total_level,max_level, min_level,npcid,r);
}

void 
gplayer_imp::ReceiveExp(const msg_exp_t & entry)
{
	int exp = entry.exp;
	int sp = entry.sp;
	float exp_adj = 0,sp_adj = 0;
	player_template::GetExpPunishment(_basic.level - entry.level,&exp_adj,&sp_adj);
	exp = (int)(exp * exp_adj + 0.5f);
	sp = (int)(sp * sp_adj + 0.5f);
	
	if(world_manager::AntiWallow())
	{
		anti_wallow::AdjustNormalExpSP(_wallow_level, exp, sp);
	}
	
	if(exp + sp) ReceiveExp(exp,sp);
}

void 
gplayer_imp::IncExp(int &exp ,int &sp,float double_exp_sp_factor, bool double_sp)
{
	if(double_exp_sp_factor > 1.00001f || double_exp_sp_factor < 0.99999f)
	{
		exp = (int)(exp * double_exp_sp_factor);
		sp = (int)(sp * double_exp_sp_factor);
	}
	else if(double_sp)
	{
		sp = (int)(sp * DOUBLE_EXP_FACTOR);
	}
	if(sp > 2000000000 - _basic.skill_point) sp = 2000000000 - _basic.skill_point;
	_basic.skill_point += sp; 
	SetRefreshState();

	if(_player_reincarnation.IsTomeActive())
	{
		//ת�����ἤ��ʱ����������
	}
	else
	{
		if(_basic.level >= player_template::GetMaxLevel())
		{	
			//�ﵽ��󼶱�
			exp = 0;
		}
	}

	if(sp || exp)
	{
		if(double_exp_sp_factor > 1.00001f || double_exp_sp_factor < 0.99999f)
		{
			GLog::log(GLOG_INFO,"�û�%d�õ�����(���鱶��%f) %d/%d",_parent->ID.id, double_exp_sp_factor, (int)exp,(int)sp);
		}
		else
		{
			if(double_sp)
			{
				GLog::log(GLOG_INFO,"�û�%d�õ�����(˫��Ԫ��) %d/%d",_parent->ID.id,(int)exp,(int)sp);
			}
			else
			{
				GLog::log(GLOG_INFO,"�û�%d�õ����� %d/%d",_parent->ID.id,(int)exp,(int)sp);
			}
		}
	}

	if(!exp) return;
	if(_player_reincarnation.IsTomeActive())
	{
		//ת�����ἤ��ʱ����������
		_player_reincarnation.IncTomeExp(exp);
	}
	else
	{
		_basic.exp += exp;
		int next_exp = player_template::GetLvlupExp(GetPlayerClass(),_basic.level);
		if(next_exp > _basic.exp)
		{
			return ;
		}
		LevelUp();
	}
}

void 
gplayer_imp::ReceiveExp(int exp,int sp)
{
	ASSERT(exp >=0 && sp >=0);
	//���㾭�飬�Ծ����������
	//������װ������ӳ�
	exp += (int)((exp * _exp_addon * 0.01f) + 0.1f);
	
	//����˫�������ж�
	float double_exp_sp_factor = 1.0f;
	if(world_manager::GetWorldParam().double_exp) 
		double_exp_sp_factor = world_manager::GetDoubleExpFactor();
	else if(_double_exp_mode) 
		double_exp_sp_factor = DOUBLE_EXP_FACTOR;
	
	//�౶�������	
	double_exp_sp_factor += _multi_exp_ctrl.GetExpFactor();
	//ת���������
	double_exp_sp_factor += _player_reincarnation.GetExpBonus();
	
	IncExp(exp,sp,
			double_exp_sp_factor, 
			world_manager::GetWorldParam().double_sp);
	_runner->receive_exp(exp,sp);
	//С�����þ���
	if(exp/10 > 0)
		ElfReceiveExp((unsigned int)(exp/10));
	return;
}

int
gplayer_imp::DoSwitch(const MSG & msg)
{
	ASSERT(msg.source.type == GM_TYPE_SERVER);
	if(msg.source.type != GM_TYPE_SERVER || msg.source.id != _switch_dest || msg.content_length != sizeof(_instance_switch_key)) 
	{
		GLog::log(GLOG_WARNING,"switch:����ķ�����������Ҫ��ת���û�(%d)������",_parent->ID.id);
		return 0;
	}

	//���key�Ƿ�ƥ�� �����ƥ�䣬ֱ�ӷ���
	if(memcmp(&_instance_switch_key, msg.content, sizeof(_instance_switch_key)))
	{
		//key��ƥ��
		GLog::log(GLOG_WARNING,"switch:����ķ�����������Ҫ��ת���û�(%d)������(Key��ƥ��)",_parent->ID.id);
		return 0;
	}

	timeval tv;
	gettimeofday(&tv,NULL);
	__PRINTF("�û�%d��ʼ������ݽ��з�����ת��:%u.%u\n",_parent->ID.id,tv.tv_sec,tv.tv_usec);
	
	int tag = msg.param;
	if(tag != world_manager::GetWorldTag() || _switch_pos.squared_distance(_parent->pos) > 125.f*125.f)
	{
		//���ڳ�Զ����ת�ƣ�����һ����ʧ����
		_runner->disappear();
	}
	
	PlayerLeaveServer();

	//���ת��Ŀ�겻�ǵ�ǰ��tag�� ��ô�����ǰ��session
	if(tag != world_manager::GetWorldTag())
	{
		ClearSession();
		ClearAction();
		//ɾ���л������������filter
		_filters.ClearSpecFilter(filter::FILTER_MASK_NOSAVE);
	}

	//���û������ݴ�������ͳ�ȥ
	gplayer * pPlayer = GetParent();
	raw_wrapper wrapper(4096);
	wrapper.SetLimit(raw_wrapper::SAVE_ONLY);
	wrapper << _instance_switch_key << world_manager::GetWorldTag();
	pPlayer->Export(wrapper);
	WrapObject(wrapper,_commander,this,_runner);

	MSG reply;
	BuildMessage(reply, GM_MSG_SWITCH_USER_DATA,msg.source,msg.target,
			 _switch_pos,_instance_switch_key.target.key_level1,wrapper.data(),wrapper.size());
	_plane->SendRemoteMessage(msg.source.id,reply);

	//���г����֪ͨ����
	_petman.PreSwitchServer(this);
	_plantpetman.PreSwitchServer(this);

	//���session
	ResetSession();

	//�ڱ�����������û����ⲿ����
	//ֻ��Ŀ��gs�뵱ǰgs����ͬһ����ͼ�Ž��д˲��� 20110117
	if(tag == world_manager::GetWorldTag())
	{
		extern_object_manager::object_appear app;
		app.body_size = pPlayer->body_size;
		app.race = pPlayer->base_info.race;
		app.faction = pPlayer->base_info.faction;
		app.level= pPlayer->base_info.level;
		app.hp= pPlayer->base_info.hp;
		app.state = pPlayer->IsZombie();
		app.where = _switch_dest;
		_plane->GetExtObjMan().Refresh(pPlayer->ID.id,_switch_pos,app);
	}

	
	//�ͷ��Լ�������
	_commander->Release();
	return 0;
}

void 
gplayer_imp::FromSwitchToNormal()
{
	ASSERT(_player_state == PLAYER_WAIT_SWITCH);

	_player_state = PLAYER_STATE_NORMAL;
	_switch_dest = -1;

	if(_parent->b_disconnect)
	{
		//����Ѿ�����
		_player_state = PLAYER_DISCONNECT;
		_disconnect_timeout = LOGOUT_TIME_IN_NORMAL;
	}
}

bool 
gplayer_imp::CanAttack(const XID & target)
{
	//�����κεط������Խ�����ͨ����
	if(!_layer_ctrl.CheckAttack()) return false;
	return _equipment[item::EQUIP_INDEX_WEAPON].CheckAttack(_equipment);
}

bool
gplayer_imp::CheckLevitate()
{
	return _ph_control.CheckLevitate();	
}

void 
gplayer_imp::PhaseControlInit()
{
	return _ph_control.Initialize(this);
}

const XID &
gplayer_imp::GetCurTarget()
{
	return ((gplayer_controller*)_commander)->GetCurTarget();
}

int 
gplayer_imp::GetAmmoCount()
{
	return _equipment[item::EQUIP_INDEX_PROJECTILE].count;
}

int 
gplayer_imp::DoAttack(const XID & target, char force_attack)
{
	if(!IsAttackMonster() && target.type == GM_TYPE_NPC && !target.IsPet())
	{
		SetAttackMonster(true);
	}

	ActiveCombatState(true);
	_combat_timer = MAX_COMBAT_TIME;
	
	unsigned char dec_arrow = 0;
	if(_cur_item.weapon_type == 1)	//Զ������
	{
		//��ͼ���ټ�֧
		int rst = _equipment.DecAmount(item::EQUIP_INDEX_PROJECTILE,1);
		dec_arrow = 1;
		__PRINTF("���ټ�֧ %d\n",rst);
	}
	
	//��乥����Ϣ����������װ���;úͼ��ټ�֧
	attack_msg attack;
	MakeAttackMsg(attack,force_attack);
	FillAttackMsg(target,attack,dec_arrow);

	//����̫�죿����������!!!
	if(_cur_prop.attack_speed <= 5)
	{
		attack.physic_damage = (int)(attack.physic_damage * player_template::GetDmgAdjToSpecAtkSpeed());
		attack.attack_rate = (int)(attack.attack_rate * player_template::GetAtkRateAdjToSpecAtkSpeed());
	}
	
	MSG msg;
	BuildMessage(msg,GM_MSG_ATTACK,target,_parent->ID,_parent->pos,
			0,&attack,sizeof(attack));
	TranslateAttack(target,attack);
	_plane->PostLazyMessage(msg);

	//����ŭ��
	if(_ap_per_hit > 0) ModifyAP(_ap_per_hit);
	return 0;
}

void 
gplayer_imp::FillAttackMsg(const XID & target,attack_msg & attack,int dec_arrow)
{
	gactive_imp::FillAttackMsg(target,attack);
	attack.weapon_class = _cur_item.weapon_class;
	attack.ainfo.sid = GetParent()->cs_sid;
	attack.ainfo.cs_index = GetParent()->cs_index;
	if(!_pvp_enable_flag) attack.force_attack = 0; //���û�д�PK���أ���ر�ǿ�ƹ���ѡ��
	if(attack.force_attack) attack.force_attack |= C2S::FORCE_ATTACK;
	if(attack.force_attack & (C2S::FORCE_ATTACK_NO_MAFIA|C2S::FORCE_ATTACK_NO_MAFIA_ALLIANCE)) 
	{
		//��������˲��˰��ɳ�Ա/����ͬ����������Ӧ��־
		attack.ainfo.mafia_id = ((gplayer*)_parent)->id_mafia;
	}
	if(attack.force_attack & C2S::FORCE_ATTACK_NO_SAME_FORCE)
	{
		attack.ainfo.force_id = _player_force.GetForce();
	}

	//����ģʽ�²���������PVP��־
	if(_pvp_enable_flag && target.IsPlayerClass() && !GetParent()->IsDuelMode())
	{
		SetPVPCombatState();
	}

	attack.attacker_mode = _pvp_enable_flag?attack_msg::PVP_ENABLE:0; 
	if(IsInPVPCombatStateHigh()) attack.attacker_mode |= attack_msg::PVP_DURATION;
	if(_task_mask & TASK_MASK_KILL_PLAYER) attack.attacker_mode |= attack_msg::PVP_FEEDBACK_KILL;

	_team.GetTeamID(attack.ainfo.team_id, attack.ainfo.team_seq);
	attack.ainfo.eff_level = _team.GetEffLevel();
	attack.ainfo.wallow_level = _team.GetWallowLevel();
	attack.ainfo.profit_level = _profit_level;
	DoWeaponOperation<0>();
	_runner->attack_once(dec_arrow);	
	__PRINTF("�����֧%d\n",dec_arrow);
}

void 
gplayer_imp::FillEnchantMsg(const XID & target, enchant_msg & enchant)
{
	gactive_imp::FillEnchantMsg(target,enchant);
	enchant.ainfo.sid = GetParent()->cs_sid;
	enchant.ainfo.cs_index = GetParent()->cs_index;
	if(!_pvp_enable_flag) enchant.force_attack = 0;//���û�д�PK���أ���ر�ǿ�ƹ���ѡ��
	if(enchant.force_attack) enchant.force_attack |= C2S::FORCE_ATTACK;
	if(enchant.force_attack & (C2S::FORCE_ATTACK_NO_MAFIA|C2S::FORCE_ATTACK_NO_MAFIA_ALLIANCE)) 
	{
		//��������˲��˰��ɳ�Ա/����ͬ����������Ӧ��־
		enchant.ainfo.mafia_id = ((gplayer*)_parent)->id_mafia;
	}
	if(enchant.force_attack & C2S::FORCE_ATTACK_NO_SAME_FORCE)
	{
		enchant.ainfo.force_id = _player_force.GetForce();
	}

	//����ģʽ�²���������PVP��־
	if(_pvp_enable_flag && target.IsPlayerClass() && !enchant.helpful && !GetParent()->IsDuelMode())
	{
		SetPVPCombatState();
	}

	enchant.attacker_mode = _pvp_enable_flag?attack_msg::PVP_ENABLE:0; 
	if(IsInPVPCombatStateHigh()) enchant.attacker_mode |= attack_msg::PVP_DURATION;
	if(_task_mask & TASK_MASK_KILL_PLAYER) enchant.attacker_mode |= attack_msg::PVP_FEEDBACK_KILL;


	_team.GetTeamID(enchant.ainfo.team_id, enchant.ainfo.team_seq);
	enchant.ainfo.eff_level = _team.GetEffLevel();
	enchant.ainfo.wallow_level = _team.GetWallowLevel();
	enchant.ainfo.profit_level = _profit_level;

	//DoWeaponOperation<0>();  ���ﲻ�ܵ��ã���������
}

void 
gplayer_imp::SendAttackMsg(const XID & target, attack_msg & attack)
{
	MSG msg;
	BuildMessage(msg,GM_MSG_ATTACK,target,_parent->ID,_parent->pos,
			0,&attack,sizeof(attack));
	TranslateAttack(target,attack);
	_plane->PostLazyMessage(msg);
}

void 
gplayer_imp::SendEnchantMsg(int message,const XID & target, enchant_msg & attack)
{
	SendTo<0>(message,target,0,&attack,sizeof(attack));
}

void
gplayer_imp::SwitchSvr(int dest, const A3DVECTOR & oldpos, const A3DVECTOR &newpos, const instance_key * switch_key)
{
	__PRINTF("switch\n");
	if(_player_state == PLAYER_STATE_NORMAL && _switch_dest == -1)
	{
		//�����Լ���ת��Ŀ���ת�Ƶص�
		_switch_dest = dest;
		_switch_pos = newpos;
		if(switch_key) 
			_instance_switch_key = *switch_key;
		else
			memset(&_instance_switch_key,0,sizeof(_instance_switch_key));

		user_save_data((gplayer*)_parent,NULL ,1);
		GLog::log(GLOG_INFO,"�û�%d���л���ͼǰ�����˴���",_parent->ID.id);

		
		//�����л�������ʱ��Ҳ�����д��̲����ˣ��������л���ɺ��ٽ���
		_write_timer = abase::Rand(500, 513);

		//����������ת��״̬���г�ʱ��
		_player_state = PLAYER_WAIT_SWITCH;
		_general_timeout = 10;

		//����ת�Ƶ�����
		ASSERT(dest != world_manager::GetWorldIndex());
		gplayer *pPlayer = GetParent();
		GMSV::SendSwitchServerStart(pPlayer->cs_index,pPlayer->ID.id, pPlayer->cs_sid,world_manager::GetWorldIndex(),dest,&_instance_switch_key,sizeof(_instance_switch_key));

		timeval tv;
		gettimeofday(&tv,NULL);
		__PRINTF("%d��ʼת�Ʒ�����:%u.%u\n",pPlayer->ID.id,tv.tv_sec,tv.tv_usec);
	}
}

enum
{	//��ʱ��
	MIN_SEND_COUNT = 128
};

void 
gplayer_dispatcher::begin_transfer()
{
	ASSERT(_mw.size() == 0);
	ASSERT(_nw.size() == 0);
	ASSERT(_pw.size() == 0);
	ASSERT(_self.size() == 0);
	ASSERT(_leave_list.size() == 0);
}

void 
gplayer_dispatcher::end_transfer()
{
	if(!_self.empty()){
		send_ls_msg(_header,_self.data(),_self.size());
		_self.clear();
	}
	wrapper_test<0>(_pw,_header,S2C::PLAYER_INFO_1_LIST);
	wrapper_test<0>(_mw,_header,S2C::MATTER_INFO_LIST);
	wrapper_test<0>(_nw,_header,S2C::NPC_INFO_LIST);
	if(_leave_list.size())
	{
		_tbuf.clear();
		using namespace S2C;
		CMD::Make<CMD::OOS_list>::From(_tbuf,_leave_list.size(), _leave_list.begin());
		send_ls_msg(_header,_tbuf.data(),_tbuf.size());
		_leave_list.clear();
	}
	
}

void 
gplayer_dispatcher::enter_slice(slice *  pPiece,const A3DVECTOR &pos)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * player = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enter_slice>::From(_tbuf,player,pos);

	cs_user_map map;
	pPiece->Lock();
	if(player->IsInvisible())
		gather_slice_cs_user_in_invisible(pPiece,map,player->invisible_degree,player->team_id);
	else
		gather_slice_cs_user(pPiece,map);
	pPiece->Unlock();
	
	multi_send_ls_msg(map,_tbuf,_imp->_parent->ID.id);

	//ȡ������������
	//get_slice_info(pPiece,_nw,_mw,_pw);
	get_slice_info_visible(pPiece,_nw,_mw,_pw,player->anti_invisible_degree,-1,player->team_id);
}

void 
gplayer_dispatcher::leave_slice(slice * pPiece,const A3DVECTOR &pos)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * player = (gplayer*)_imp->_parent;
	CMD::Make<CMD::leave_slice>::From(_tbuf,player);
	//�����뿪��Ϣ
	cs_user_map map;
	pPiece->Lock();
	if(player->IsInvisible())
		gather_slice_cs_user_in_invisible(pPiece,map,player->invisible_degree,player->team_id);
	else
		gather_slice_cs_user(pPiece,map);
	//gather_slice_object(pPiece,_leave_list);
	gather_slice_object_visible(pPiece,_leave_list,player->anti_invisible_degree);
	pPiece->Unlock();
	
	multi_send_ls_msg(map,_tbuf,_imp->_parent->ID.id);
}

void 
gplayer_dispatcher::start_attack(const XID & target)
{
/*
	_tbuf.clear();
	gactive_imp * pImp = (gactive_imp *)_imp;
	gplayer * pObj = (gplayer*)pImp->_parent;
	CMD::Make<CMD::object_start_attack>::From(_tbuf,pObj,target.id,pImp->_cur_prop.attack_speed);
	AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf.data(),_tbuf.size(),pObj->ID.id);
	*/

	//���з����Լ���һ������Ĺ�����ʼ��Ϣ
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp *)_imp;
	gplayer * pObj = (gplayer*)pImp->_parent;
	unsigned char speed = pImp->_cur_prop.attack_speed;
	CMD::Make<CMD::self_start_attack>::From(_tbuf,target.id,speed,pImp->GetAmmoCount());
	send_ls_msg(pObj,_tbuf);
}

void 
gplayer_dispatcher::attack_once(unsigned char state)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pObj = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_attack_once>::From(_tbuf,state);
	send_ls_msg(pObj,_tbuf);
}

void 
gplayer_dispatcher::be_damaged(const XID & id, int skill_id ,const attacker_info_t & info,int damage,int dura_index,int at_state,char speed,bool orange,unsigned char section)
{
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	if(skill_id)
	{
		if(id.type == GM_TYPE_PLAYER && info.cs_index >= 0)
		{
			_tbuf.clear();
			CMD::Make<CMD::self_skill_attack_result>::From(_tbuf,pPlayer->ID,skill_id,damage,at_state,speed ,section);
			send_ls_msg(info.cs_index,id.id,info.sid,_tbuf);
			_tbuf.clear();
			CMD::Make<CMD::object_skill_attack_result>::From(_tbuf,id,pPlayer->ID,skill_id,damage,at_state ,speed ,section);
			AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,id.id,pPlayer->ID.id,0);
		}
		else
		{
			_tbuf.clear();
			CMD::Make<CMD::object_skill_attack_result>::From(_tbuf,id,pPlayer->ID,skill_id,damage,at_state ,speed ,section);
			AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
		}

		_tbuf.clear();
		CMD::Make<CMD::be_skill_attacked>::From(_tbuf,id,skill_id,damage,dura_index,orange,at_state,speed, section);
		send_ls_msg(pPlayer,_tbuf);
	}
	else
	{
		if(id.type == GM_TYPE_PLAYER && info.cs_index >= 0)
		{
			_tbuf.clear();
			CMD::Make<CMD::self_attack_result>::From(_tbuf,pPlayer->ID,damage,at_state,speed);
			send_ls_msg(info.cs_index,id.id,info.sid,_tbuf);
			_tbuf.clear();
			CMD::Make<CMD::object_attack_result>::From(_tbuf,id,pPlayer->ID,damage,at_state ,speed);
			AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,id.id,pPlayer->ID.id,0);
		}
		else
		{
			_tbuf.clear();
			CMD::Make<CMD::object_attack_result>::From(_tbuf,id,pPlayer->ID,damage,at_state ,speed);
			AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
		}

		_tbuf.clear();
		CMD::Make<CMD::be_attacked>::From(_tbuf,id,damage,dura_index,orange,at_state,speed);
		send_ls_msg(pPlayer,_tbuf);
	}
}

void 
gplayer_dispatcher::be_hurt(const XID & id, const attacker_info_t & info,int damage,bool invader)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	if(id.type == GM_TYPE_PLAYER && info.cs_index >= 0)
	{
		//����Է�����ң������ض����ݸ���ң���������$$$$$��
		//��Ҫ���������˹����Ľ���ģ�������������������������
		CMD::Make<CMD::hurt_result>::From(_tbuf,pPlayer->ID,damage);
		send_ls_msg(info.cs_index,id.id,info.sid,_tbuf);
		_tbuf.clear();
	}

	CMD::Make<CMD::be_hurt>::From(_tbuf,id,damage,invader);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::dodge_attack(const XID &attacker, int skill_id, const attacker_info_t & info, int at_state,char speed,bool orange,unsigned char section)
{
	using namespace S2C;
	gplayer *pObj = (gplayer*)_imp->_parent;
	if(skill_id)
	{
		_tbuf.clear();
		if(attacker.type == GM_TYPE_PLAYER && info.cs_index >= 0)
		{
			CMD::Make<CMD::object_skill_attack_result>::From(_tbuf,attacker,pObj->ID,skill_id,0,at_state,speed,section);
			AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,pObj->ID.id,attacker.id,0);

			_tbuf.clear();
			CMD::Make<CMD::self_skill_attack_result>::From(_tbuf,pObj->ID,skill_id,0,at_state,speed,section);
			send_ls_msg(info.cs_index,attacker.id,info.sid,_tbuf);

		}
		else
		{
			CMD::Make<CMD::object_skill_attack_result>::From(_tbuf,attacker,pObj->ID,skill_id,0,at_state,speed,section);
			AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,pObj->ID.id);
		}

		_tbuf.clear();
		CMD::Make<CMD::be_skill_attacked>::From(_tbuf,attacker,skill_id,0,0xff,orange,at_state,speed, section);
		send_ls_msg(pObj,_tbuf);
	}
	else
	{
		_tbuf.clear();
		if(attacker.type == GM_TYPE_PLAYER && info.cs_index >= 0)
		{
			CMD::Make<CMD::object_attack_result>::From(_tbuf,attacker,pObj->ID,0,at_state,speed);
			AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,pObj->ID.id,attacker.id,0);

			_tbuf.clear();
			CMD::Make<CMD::self_attack_result>::From(_tbuf,pObj->ID,0,at_state,speed);
			send_ls_msg(info.cs_index,attacker.id,info.sid,_tbuf);

		}
		else
		{
			CMD::Make<CMD::object_attack_result>::From(_tbuf,attacker,pObj->ID,0,at_state,speed);
			AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,pObj->ID.id);
		}

		_tbuf.clear();
		CMD::Make<CMD::be_attacked>::From(_tbuf,attacker,0,0xff,orange,at_state,speed);
		send_ls_msg(pObj,_tbuf);
	}
}

void
gplayer_dispatcher::equipment_damaged(int index,char reason)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::equipment_damaged>::From(_tbuf,index,reason);
	send_ls_msg((gplayer*)_imp->_parent,_tbuf);
}

void 
gplayer_dispatcher::on_death(const XID &killer,bool )
{
	_tbuf.clear();
	using namespace S2C;
	gobject * pObj = _imp->_parent;
	
	CMD::Make<CMD::be_killed>::From(_tbuf,pObj,killer);
	send_ls_msg((gplayer*)_imp->_parent,_tbuf);
	__PRINTF("send be killed message\n");
	
	_tbuf.clear();
	CMD::Make<CMD::player_dead>::From(_tbuf,killer,pObj->ID);
	AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,pObj->ID.id);
}

void 
gplayer_dispatcher::stop_attack(int flag)
{
	timeval tv;
	gettimeofday(&tv,NULL);
	__PRINTF("player stop attack %ld.%06ld\n",tv.tv_sec,tv.tv_usec);

	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::self_stop_attack>::From(_tbuf,flag);

	gobject * pObj = _imp->_parent;
	send_ls_msg((gplayer*)pObj,_tbuf);
}

void 
gplayer_dispatcher::resurrect(int level)
{	
//	timeval tv;
//	gettimeofday(&tv,NULL);
//	__PRINTF("%d.%06d player resurrect\n",tv.tv_sec,tv.tv_usec);
	_tbuf.clear();
	using namespace S2C;
	gobject * pObj = _imp->_parent;
	CMD::Make<CMD::player_revival>::From(_tbuf,pObj,level);
	//����Ҫ��������֪��
	AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,-1);//�Լ�Ҳ�����յ������Ϣ
//	gettimeofday(&tv,NULL);
//	__PRINTF("%d.%06d player resurrect\n",tv.tv_sec,tv.tv_usec);
}

void 
gplayer_dispatcher::notify_move(const A3DVECTOR &oldpos, const A3DVECTOR & newpos)
{
	//��ʱ�Ȳ������ڱ߽����������
	rect rt(oldpos,newpos);

	packet_raw_wrapper d1(128);
	packet_wrapper l1(64),e1(64); 	
	//���ﱣ����ǵ���Ҫ�������������͸�link server�����ݣ�������Ȼ��Ҫ���п��ܵı���
	using namespace S2C;
	CMD::Make<CMD::player_enter_slice>::From(e1,(gplayer*)_imp->_parent,newpos);
	CMD::Make<CMD::leave_slice>::From(l1,_imp->_parent);

	msg_usermove_t mt;
	mt.cs_index = _header.cs_id;
	mt.cs_sid =  _header.cs_sid;
	mt.user_id = _header.user_id;
	mt.newpos = newpos;
	mt.leave_data_size = l1.size();
	mt.enter_data_size = e1.size();
	d1.push_back(&mt,sizeof(mt));
	//mt.Export(e1);
	d1.push_back(l1.data(),l1.size());
	d1.push_back(e1.data(),e1.size());

	MSG msg;
	BuildMessage(msg,GM_MSG_USER_MOVE_OUTSIDE,XID(GM_TYPE_BROADCAST,-1),_imp->_parent->ID,
					oldpos,0,d1.data(),d1.size());
	_imp->_plane->BroadcastSvrMessage(rt,msg,GRID_SIGHT_RANGE);
}

void 
gplayer_dispatcher::notify_pos(const A3DVECTOR & pos)
{	
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::notify_pos>::From(_tbuf,pos,world_manager::GetWorldTag(),
			world_manager::GetInstance()->GetWorldType() == WORLD_TYPE_PARALLEL_WORLD ? _imp->_plane->w_ins_key : instance_hash_key());
	send_ls_msg((gplayer*)(_imp->_parent),_tbuf);
}

void 
gplayer_dispatcher::get_base_info()
{
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::self_info_1>::From(_self,(gplayer*)pImp->_parent,pImp->_basic.exp,pImp->_basic.skill_point);

	//_imp->GetBaseInfo(_self);
}

void 
gplayer_dispatcher::query_info00(const XID & target, int cs_index,int sid)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp *)_imp;
	//_back_up_hp[1]
	CMD::Make<CMD::player_info_00>::From(_tbuf,pImp->_parent->ID,pImp->_basic.hp,pImp->_basic,pImp->_cur_prop,pImp->IsCombatState()?1:0, pImp->GetCurTarget().id); // todo ddr
	send_ls_msg(cs_index,target.id,sid,_tbuf);
}

void 
gplayer_dispatcher::query_info00()
{
/*
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::player_info_00>::From(_tbuf,pImp->_parent->ID,pImp->_basic,pImp->_cur_prop);
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
	*/
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::self_info_00>::From(_tbuf,pImp->_basic,pImp->_cur_prop,pImp->IsCombatState()?1:0,
			pImp->_cheat_punish);
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
}
//lgc
void gplayer_dispatcher::query_elf_vigor()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::elf_vigor>::From(_tbuf, (int)pImp->_cur_elf_info.vigor, (int)pImp->_cur_elf_info.max_vigor, int(pImp->_cur_elf_info.vigor_gen*100+0.5f));
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
}

void gplayer_dispatcher::query_elf_enhance()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::elf_enhance>::From(_tbuf,
						pImp->_cur_elf_info.final_strength - pImp->_cur_elf_info.strength,
						pImp->_cur_elf_info.final_agility - pImp->_cur_elf_info.agility, 
						pImp->_cur_elf_info.final_vitality - pImp->_cur_elf_info.vitality, 
						pImp->_cur_elf_info.final_energy - pImp->_cur_elf_info.energy);
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
}

void gplayer_dispatcher::query_elf_stamina(int sta)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::elf_stamina>::From(_tbuf, sta);
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
}

void gplayer_dispatcher::query_elf_exp(int exp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::elf_exp>::From(_tbuf, exp);
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
}

void gplayer_dispatcher::elf_cmd_result(int cmd, int result, int param1, int param2)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::elf_cmd_result>::From(_tbuf, cmd, result, param1, param2);
	send_ls_msg((gplayer *)pImp->_parent,_tbuf);
}

void gplayer_dispatcher::elf_levelup()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_elf_levelup>::From(_tbuf, pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::move(const A3DVECTOR & target, int cost_time,int speed,unsigned char move_mode)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_move>::From(_tbuf,pPlayer,target,cost_time,speed,move_mode);

	slice * pPiece = pPlayer->pPiece;
	
	if(pPlayer->IsInvisible())
		AutoBroadcastCSMsgInInvisible(_imp->_plane,pPiece,_tbuf,pPlayer->invisible_degree,pPlayer->team_id,pPlayer->ID.id);
	else
		AutoBroadcastCSMsg(_imp->_plane,pPiece,_tbuf,pPlayer->ID.id);//�Լ�Ҳ�����յ������Ϣ
}

void
gplayer_dispatcher::stop_move(const A3DVECTOR & target, unsigned short speed ,unsigned char dir,unsigned char move_mode)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_stop_move>::From(_tbuf,pPlayer,target,speed,dir,move_mode);

	pPlayer->dir = dir;
	slice * pPiece = pPlayer->pPiece;

	if(pPlayer->IsInvisible())
		AutoBroadcastCSMsgInInvisible(_imp->_plane,pPiece,_tbuf,pPlayer->invisible_degree,pPlayer->team_id,pPlayer->ID.id);
	else
		AutoBroadcastCSMsg(_imp->_plane,pPiece,_tbuf,pPlayer->ID.id);
}


void
gplayer_dispatcher::leave_world()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)(_imp->_parent);
	world * pPlane = _imp->_plane;
	CMD::Make<CMD::player_leave_world>::From(_tbuf,pPlayer);
	slice *pPiece = pPlayer->pPiece;
	if(pPlayer->IsInvisible())
		AutoBroadcastCSMsgInInvisible(_imp->_plane,pPiece,_tbuf,pPlayer->invisible_degree,pPlayer->team_id,pPlayer->ID.id);
	else
		AutoBroadcastCSMsg(pPlane,pPiece,_tbuf,pPlayer->ID.id);

	//��ʱ�Ȳ������ڱ߽����������
	if(pPiece->IsBorder())
	{
		extern_object_manager::SendDisappearMsg<0>(_imp->_plane,pPlayer,pPiece);
	}
}

void 
gplayer_dispatcher::enter_region()
{
	world * pPlane = _imp->_plane;
	slice * pPiece = _imp->_parent->pPiece;
	//get_slice_info(pPiece,_nw,_mw,_pw);
	gplayer * pPlayer = (gplayer*)(_imp->_parent);
	get_slice_info_visible(pPiece,_nw,_mw,_pw,pPlayer->anti_invisible_degree,-1,pPlayer->team_id);

	int i;
	int total = pPlane->w_far_vision;
	int index = pPlane->GetGrid().GetSliceIndex(pPiece);
	int slice_x,slice_z;
	pPlane->GetGrid().Index2Pos(index,slice_x,slice_z);
	for(i = 0; i <total; i ++)
	{
		world::off_node_t &node = pPlane->w_off_list[i]; 
		int nx = slice_x + node.x_off; 
		int nz = slice_z + node.z_off; 
		if(nx < 0 || nz < 0 || nx >= pPlane->GetGrid().reg_column || nz >= pPlane->GetGrid().reg_row) continue;
		slice * pNewPiece = pPiece + node.idx_off;
		if(i <= pPlane->w_true_vision)
		{
			//get_slice_info(pNewPiece,_nw,_mw,_pw);
			get_slice_info_visible(pNewPiece,_nw,_mw,_pw,pPlayer->anti_invisible_degree,-1,pPlayer->team_id);
		}
		else
		{
			//get_slice_info(pNewPiece,_nw,_mw,_pw);
			get_slice_info_visible(pNewPiece,_nw,_mw,_pw,pPlayer->anti_invisible_degree,-1,pPlayer->team_id);
		}
		wrapper_test<MIN_SEND_COUNT>(_pw,_header,S2C::PLAYER_INFO_1_LIST);
		wrapper_test<MIN_SEND_COUNT>(_mw,_header,S2C::MATTER_INFO_LIST);
		wrapper_test<MIN_SEND_COUNT>(_nw,_header,S2C::NPC_INFO_LIST);
	}
	return ;
}

void 
gplayer_dispatcher::enter_world()
{
	world * pPlane = _imp->_plane;
	//���ȷ��ͽ����������Ϣ
	_tbuf.clear();
	using namespace S2C;
	gplayer *pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enter_world>::From(_tbuf,pPlayer);
	slice * pPiece = pPlayer->pPiece;
	if(pPlayer->IsInvisible())
		AutoBroadcastCSMsgInInvisible(_imp->_plane,pPiece,_tbuf,pPlayer->invisible_degree,pPlayer->team_id,pPlayer->ID.id);
	else
		AutoBroadcastCSMsg(pPlane,pPiece,_tbuf,pPlayer->ID.id);

	//��ʱ�Ȳ������ڱ߽����������
	if(pPiece->IsBorder())
	{
		extern_object_manager::SendAppearMsg<0>(_imp->_plane,pPlayer,pPiece);
		MSG msg;
		BuildMessage(msg,GM_MSG_USER_APPEAR_OUTSIDE,XID(GM_TYPE_BROADCAST,-1),pPlayer->ID,
				pPlayer->pos,pPlayer->cs_index,&(pPlayer->cs_sid),sizeof(int));
		_imp->_plane->BroadcastSvrMessage(pPiece->slice_range,msg,GRID_SIGHT_RANGE);  //������Ұ�Ĺ㲥��Χ
	}

	//ȡ�ý���һ������Ӧ��ȡ������
	enter_region();
}

void 
gplayer_dispatcher::appear()
{
	world * pPlane = _imp->_plane;
	//���ȷ��ͽ����������Ϣ
	_tbuf.clear();
	using namespace S2C;
	gplayer *pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enter_world>::From(_tbuf,pPlayer);
	slice * pPiece = pPlayer->pPiece;
	if(pPlayer->IsInvisible())
		AutoBroadcastCSMsgInInvisible(pPlane,pPiece,_tbuf,pPlayer->invisible_degree,pPlayer->team_id,pPlayer->ID.id);
	else
		AutoBroadcastCSMsg(pPlane,pPiece,_tbuf,pPlayer->ID.id);
}

void 
gplayer_dispatcher::error_message(int msg)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::error_msg>::From(_tbuf,msg);
	//�����뿪��Ϣ
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void
gplayer_dispatcher::disappear()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer *)_imp->_parent;
	CMD::Make<CMD::object_disappear>::From(_tbuf,pPlayer);
	slice * pPiece = pPlayer->pPiece;
	if(pPlayer->IsInvisible())
		AutoBroadcastCSMsgInInvisible(_imp->_plane,pPiece,_tbuf,pPlayer->invisible_degree,pPlayer->team_id,pPlayer->ID.id);
	else
		AutoBroadcastCSMsg(_imp->_plane,pPiece,_tbuf,pPlayer->ID.id);

	//��ʱ�Ȳ������ڱ߽����������
	if(pPiece->IsBorder())
	{
		extern_object_manager::SendDisappearMsg<0>(_imp->_plane,pPlayer,pPiece);
	}
}

void 
gplayer_dispatcher::receive_exp(int exp,int sp)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::receive_exp>::From(_tbuf,exp,sp);
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void
gplayer_dispatcher::embed_item(size_t chip_idx, size_t equip_idx)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::embed_item>::From(_tbuf,chip_idx,equip_idx);
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void 
gplayer_dispatcher::level_up()
{
	_tbuf.clear();
	using namespace S2C;
	gobject * pObj = _imp->_parent;
	CMD::Make<CMD::level_up>::From(_tbuf,pObj);
	slice * pPiece = pObj->pPiece;
	AutoBroadcastCSMsg(_imp->_plane,pPiece,_tbuf);
}

void 
gplayer_dispatcher::unselect()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::unselect>::From(_tbuf);
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void 
gplayer_dispatcher::player_select_target(int id)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_select_target>::From(_tbuf,id);
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void 
gplayer_dispatcher::get_extprop_base()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::player_extprop_base>::From(_tbuf,pImp);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
	
}

void 
gplayer_dispatcher::get_extprop_move()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::player_extprop_move>::From(_tbuf,pImp);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::get_extprop_attack()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::player_extprop_attack>::From(_tbuf,pImp);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::get_extprop_defense()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::player_extprop_defense>::From(_tbuf,pImp);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::player_reject_invite(const XID & member)
{
	__PRINTF("team::player_reject_invite %d\n",_imp->_parent->ID.id);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_reject_invite>::From(_tbuf,member);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::leader_invite(const XID & leader,int seq,int pickup_flag)
{
	__PRINTF("team::leader_invite %d  seq %d\n",_imp->_parent->ID.id,seq);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_leader_invite>::From(_tbuf,leader,seq,pickup_flag);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::join_team(const XID & leader,int pickup_flag)
{
	__PRINTF("team::join_team %d\n",_imp->_parent->ID.id);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_join_team>::From(_tbuf,leader,pickup_flag);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::member_leave(const XID & leader, const XID& member,int type)
{
	__PRINTF("team::member_leave %d\n",_imp->_parent->ID.id);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_member_leave>::From(_tbuf,leader,member,type);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::leave_party(const XID & leader, int type)
{
	__PRINTF("team::leave_party %d\n",_imp->_parent->ID.id);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_leave_party>::From(_tbuf,leader,type);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::new_member(const XID & member)
{
	__PRINTF("team::new_member %d\n",_imp->_parent->ID.id);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_new_member>::From(_tbuf,member);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::leader_cancel_party(const XID & leader)
{
	//�������Ѿ�ȡ��
	ASSERT(false && "�������Ѿ�ȡ��");
	__PRINTF("team::leader_cancel_party %d\n",_imp->_parent->ID.id);
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_leader_cancel_party>::From(_tbuf,leader);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::npc_greeting(const XID & provider)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::npc_greeting>::From(_tbuf,provider);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::repair_all(size_t cost)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::repair_all>::From(_tbuf,cost);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::repair(int where,int index, size_t cost)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::repair>::From(_tbuf,where,index,cost);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::renew()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::renew>::From(_tbuf);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::spend_money(size_t cost)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::spend_money>::From(_tbuf,cost);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::get_player_money(size_t money,size_t capacity)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::get_own_money>::From(_tbuf,money,capacity);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::cast_skill(const XID & target, int skill,unsigned short time, unsigned char level)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	gplayer * pPlayer = (gplayer*)pImp->_parent;
	CMD::Make<CMD::object_cast_skill>::From(_tbuf,pPlayer->ID,target,skill,time,level);
	AutoBroadcastCSMsg(pImp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::cast_rune_skill(const XID & target, int skill,unsigned short time, unsigned char level)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	gplayer * pPlayer = (gplayer*)pImp->_parent;
	CMD::Make<CMD::player_cast_rune_skill>::From(_tbuf,pPlayer->ID,target,skill,time,level);
	AutoBroadcastCSMsg(pImp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::skill_interrupt(char reason)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	gplayer * pPlayer = (gplayer*)pImp->_parent;
	CMD::Make<CMD::skill_interrupted>::From(_tbuf,pPlayer->ID);
	AutoBroadcastCSMsg(pImp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);

	_tbuf.clear();
	CMD::Make<CMD::self_skill_interrupted>::From(_tbuf,reason);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::stop_skill()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::self_stop_skill>::From(_tbuf);
	send_ls_msg(pPlayer, _tbuf);
}


void
gplayer_dispatcher::takeoff()
{
	_tbuf.clear();
	using namespace S2C;
	gobject * pObj= (gobject*)_imp->_parent;
	CMD::Make<CMD::object_takeoff>::From(_tbuf,pObj);
	AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::landing()
{
	_tbuf.clear();
	using namespace S2C;
	gobject * pObj= (gobject*)_imp->_parent;
	CMD::Make<CMD::object_landing>::From(_tbuf,pObj);
	AutoBroadcastCSMsg(_imp->_plane,pObj->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::flysword_time_capacity(unsigned char where, unsigned char index, int cur_time)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::flysword_time_capacity>::From(_tbuf,where,index,cur_time);
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void 
gplayer_dispatcher::skill_perform()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	gplayer * pPlayer = (gplayer*)pImp->_parent;
	CMD::Make<CMD::skill_perform>::From(_tbuf);
//	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1); Ŀǰû�й㲥
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::teammate_get_pos(const XID & target,const A3DVECTOR & pos,int tag,bool same_plane) 
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::teammate_pos>::From(_tbuf,target,pos,tag,(same_plane?1:0));
	send_ls_msg((gplayer*)_imp->_parent,_tbuf);
}

void 
gplayer_dispatcher::send_equipment_info(const XID & target, int cs_index, int sid)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	gplayer * pPlayer = (gplayer*)pImp->_parent;
	CMD::Make<CMD::send_equipment_info>::From(_tbuf,pPlayer,pImp->_equip_info.mask,pImp->_equip_info.data);
	send_ls_msg(cs_index,target.id,sid,_tbuf);
}

void 
gplayer_dispatcher::equipment_info_changed(uint64_t madd,uint64_t mdel,const void * buf, size_t size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	gplayer * pPlayer = (gplayer*)pImp->_parent;
	CMD::Make<CMD::equipment_info_changed>::From(_tbuf,pPlayer,madd,mdel,buf,size);
	AutoBroadcastCSMsg(pImp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}

void 
gplayer_dispatcher::team_member_pickup(const XID & member, int type, int count)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::team_member_pickup>::From(_tbuf,member, type, count);
	send_ls_msg((gplayer*)(_imp->_parent), _tbuf);
}

void 
gplayer_dispatcher::send_team_data(const XID & leader,size_t team_count,size_t data_count, const player_team::member_entry ** list)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_member_data>::From(_tbuf,leader,team_count,data_count,list);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::send_team_data(const XID & leader,size_t team_count,const player_team::member_entry * list)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::team_member_data>::From(_tbuf,leader,team_count,list);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::item_to_money(size_t index, int type, size_t count,size_t price)
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp*)_imp;
	CMD::Make<CMD::item_to_money>::From(_tbuf,index,type,count,price);
	send_ls_msg((gplayer*)(pImp->_parent), _tbuf);
}

void 
gplayer_dispatcher::pickup_money(int money)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pickup_money>::From(_tbuf,money);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	if(abase::Rand(0,0) == 1)
	{
		std::pair<int/*user*/,int/*sid*/> pp(pPlayer->ID.id,pPlayer->cs_sid);
		GMSV::MultiSendClientData(pPlayer->cs_index,&pp,&pp,_tbuf.data(),_tbuf.size(),0);

	}
	else
	{
		send_ls_msg(pPlayer, _tbuf);
	}
}

void 
gplayer_dispatcher::pickup_item(int type,int expire_date, int amount, int slot_amount, int where,int index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pickup_item>::From(_tbuf,type,expire_date,amount,slot_amount,where & 0xFF,index & 0xFF);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::obtain_item(int type, int expire_date, int amount, int slot_amount, int where,int index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_obtain_item>::From(_tbuf,type, expire_date,amount,slot_amount,where & 0xFF,index & 0xFF);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::produce_once(int type, int amount, int slot_amount, int where,int index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::produce_once>::From(_tbuf,type,amount,slot_amount,where & 0xFF,index & 0xFF);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::produce_start(int type, int use_time ,int count)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::produce_start>::From(_tbuf,type,use_time,count);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::produce_end()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::produce_end>::From(_tbuf);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::decompose_start(int use_time,int type)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::decompose_start>::From(_tbuf,use_time,type);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::decompose_end()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::decompose_end>::From(_tbuf);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::pickup_money_in_trade(size_t money)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pickup_money_in_trade>::From(_tbuf,money);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::pickup_item_in_trade(int type, int amount)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pickup_item_in_trade>::From(_tbuf,type,amount);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::receive_money_after_trade(size_t money)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pickup_money_after_trade>::From(_tbuf,money);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::receive_item_after_trade(int type, int expire_date, int amount, int slot_amount,int index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pickup_item_after_trade>::From(_tbuf,type,expire_date,amount, slot_amount,index & 0xFFFF);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}


void 
gplayer_dispatcher::purchase_item(int type, size_t money,int amount, int slot_amount, int where,int index)
{
	ASSERT(false);
/*
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_purchase_item>::From(_tbuf,type,money,amount & 0xFFFF,slot_amount & 0xFFFF,where & 0xFF,index & 0xFF);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
	*/
}

void 
gplayer_dispatcher::self_item_empty_info(int where,int index) 
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::self_item_empty_info>::From(_tbuf,where, index);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::self_item_info(int where,int index,item_data & data,unsigned short crc) 
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::self_item_info>::From(_tbuf,where, index,data,crc);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::self_inventory_data(int where,unsigned char inv_size, const void * data, size_t len)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::self_inventory_data>::From(_tbuf,where,inv_size,data,len);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::self_inventory_detail_data(int where,unsigned char inv_size,const void * data, size_t len)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::self_inventory_detail_data>::From(_tbuf,where,inv_size,data,len);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::exchange_inventory_item(size_t idx1,size_t idx2)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::exchange_inventory_item>::From(_tbuf,idx1,idx2);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::move_inventory_item(size_t src,size_t dest,size_t count)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::move_inventory_item>::From(_tbuf,src,dest,count);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::player_drop_item(size_t where, size_t index,int type,size_t count, unsigned char drop_type)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_drop_item>::From(_tbuf,where,index,type,count,drop_type);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::exchange_equipment_item(size_t index1,size_t index2)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::exchange_equipment_item>::From(_tbuf,index1,index2);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::equip_item(size_t index_inv,size_t index_equip,int count_inv,int count_eq)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::equip_item>::From(_tbuf,index_inv,index_equip,count_inv,count_eq);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::move_equipment_item(size_t index_inv,size_t index_equip, size_t count)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::move_equipment_item>::From(_tbuf,index_inv,index_equip,count);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::self_get_property(size_t status_point, const extend_prop & prop , int attack_degree, int defend_degree, int crit_rate, int crit_damage_bonus, int invisible_degree, int anti_invisible_degree, int penetration, int resilience, int vigour)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::self_get_property>::From(_tbuf,status_point,prop,attack_degree, defend_degree, crit_rate, crit_damage_bonus, invisible_degree, anti_invisible_degree, penetration, resilience, vigour); 
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::set_status_point(size_t vit, size_t eng, size_t str, size_t agi, size_t remain)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::set_status_point>::From(_tbuf,vit,eng,str,agi,remain);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::get_skill_data()
{
	_tbuf.clear();
	using namespace S2C;
	gactive_imp * pImp = (gactive_imp *)_imp;
	raw_wrapper wrapper(256);
	pImp->_skill.StorePartial(wrapper);
	CMD::Make<CMD::skill_data>::From(_tbuf,wrapper.data(),wrapper.size());
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::get_task_data()
{
	//���ﲻ��tbuf����Ϊ̫����
	packet_wrapper  h1(8192);
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	PlayerTaskInterface  task_if(pImp);
	CMD::Make<CMD::task_data>::From(h1,task_if.GetActiveTaskList(),task_if.GetActLstDataSize(),task_if.GetFinishedTaskList(),task_if.GetFnshLstDataSize(), task_if.GetFinishedTimeList(), task_if.GetFnshTimeLstDataSize(), task_if.GetFinishedCntList(), task_if.GetFnshCntLstDataSize(), task_if.GetStorageTaskList(), task_if.GetStorageTaskLstDataSize());
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, h1);
}

void 
gplayer_dispatcher::send_task_var_data(const void * buf, size_t size)
{
	//���ﲻ��tbuf����Ϊ̫����
	_tbuf.clear();
	using namespace S2C;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	PlayerTaskInterface  task_if(pImp);
	CMD::Make<CMD::task_var_data>::From(_tbuf,buf,size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::clear_embedded_chip(unsigned short equip_idx,size_t use_money)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::clear_embedded_chip>::From(_tbuf,equip_idx, use_money);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::cost_skill_point(int sp)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::cost_skill_point>::From(_tbuf,sp);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::learn_skill(int skill ,int level)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::learn_skill>::From(_tbuf,skill,level);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::use_item(char where, unsigned char index , int item_type , unsigned short count)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_use_item>::From(_tbuf,where, index, item_type, count);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::use_item(char where, unsigned char index , int item_type , unsigned short count,const char * arg, size_t 
		arg_size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_use_item_with_arg>::From(_tbuf,where, index, item_type, count,arg,arg_size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf); 
}

void 
gplayer_dispatcher::use_item(int item_type,const char * arg, size_t arg_size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_use_item_with_arg>::From(_tbuf,pPlayer,item_type,arg,arg_size);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}


void 
gplayer_dispatcher::start_use_item(int item_type, int use_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_start_use>::From(_tbuf,pPlayer,item_type,use_time);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::start_use_item_with_target(int item_type, int use_time,const XID & target)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_start_use_with_target>::From(_tbuf,pPlayer,item_type,use_time,target);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::cancel_use_item()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_cancel_use>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::use_item(int item_type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_use_item>::From(_tbuf,pPlayer,item_type);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}

void 
gplayer_dispatcher::sit_down()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_sit_down>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::stand_up()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_stand_up>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::do_emote(unsigned short emotion)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_do_emote>::From(_tbuf,pPlayer,emotion);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::do_emote_restore(unsigned short emotion)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_do_emote_restore>::From(_tbuf,pPlayer,emotion);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::do_action(unsigned char action)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_do_action>::From(_tbuf,pPlayer,action);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::send_timestamp()
{
	_tbuf.clear();
	using namespace S2C;
	__PRINTF("%d %d\n",g_timer.get_systime(),time(NULL));
	gplayer * pPlayer = (gplayer*)_imp->_parent;
/*	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
*/
	time_t t1 = time(NULL);
	struct tm tm1;
	localtime_r(&t1, &tm1);
	CMD::Make<CMD::server_timestamp>::From(_tbuf,t1, -(tm1.tm_gmtoff/60), world_manager::GetLuaVersion());
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::notify_root(unsigned char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pObject =(gplayer*) _imp->_parent;
	CMD::Make<CMD::notify_root>::From(_tbuf,pObject);
	AutoBroadcastCSMsg(_imp->_plane,pObject->pPiece,_tbuf,pObject->ID.id);

	_tbuf.clear();
	CMD::Make<CMD::self_notify_root>::From(_tbuf,pObject,type&0x7F);
	send_ls_msg(pObject, _tbuf);

}

void
gplayer_dispatcher::dispel_root(unsigned char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::dispel_root>::From(_tbuf,type);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::invader_rise()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::invader_rise>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::pariah_rise()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	__PRINTF("�������֣�state %d\n",pPlayer->pariah_state);
	CMD::Make<CMD::pariah_rise>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::invader_fade()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::invader_fade>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::gather_start(int mine, unsigned char t)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_gather_start>::From(_tbuf,pPlayer->ID,mine,t);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::gather_stop()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_gather_stop>::From(_tbuf,pPlayer->ID);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::trashbox_passwd_changed(bool has_passwd)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trashbox_passwd_changed>::From(_tbuf,has_passwd);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::trashbox_open(char is_usertrashbox)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	gplayer_imp *pImp = (gplayer_imp*)_imp;
	player_trashbox * box;
	if(is_usertrashbox)
		box = &pImp->_user_trashbox;
	else
		box = &pImp->_trashbox;
	CMD::Make<CMD::trashbox_open>::From(_tbuf,is_usertrashbox,box->GetTrashBoxSize(),box->GetTrashBoxSize2(),box->GetTrashBoxSize3());
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::trashbox_close(char is_usertrashbox)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trashbox_close>::From(_tbuf,is_usertrashbox);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::trashbox_wealth(char is_usertrashbox, size_t money)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trashbox_wealth>::From(_tbuf,is_usertrashbox,money);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::trashbox_passwd_state(bool has_passwd)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trashbox_passwd_state>::From(_tbuf,has_passwd);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::exchange_trashbox_item(int where, size_t idx1, size_t idx2)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::exchange_trashbox_item>::From(_tbuf,where, idx1,idx2);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::move_trashbox_item(int where,size_t src, size_t dest, size_t delta)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::move_trashbox_item>::From(_tbuf,where, src,dest,delta);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::exchange_trashbox_inventory(int where,size_t idx_tra,size_t idx_inv)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::exchange_trashbox_inventory>::From(_tbuf,where,idx_tra,idx_inv);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::trash_item_to_inventory(int where,size_t idx_tra, size_t idx_inv, size_t delta)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trash_item_to_inventory>::From(_tbuf,where, idx_tra,idx_inv,delta);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::inventory_item_to_trash(int where,size_t idx_inv, size_t idx_tra, size_t delta)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::inventory_item_to_trash>::From(_tbuf,where, idx_inv,idx_tra,delta);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::exchange_trash_money(char is_usertrashbox, int inv_money, int tra_money)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::exchange_trash_money>::From(_tbuf,is_usertrashbox,inv_money,tra_money);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::set_adv_data(int data1,int data2)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_set_adv_data>::From(_tbuf,pPlayer,data1,data2);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::clear_adv_data()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_clr_adv_data>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_in_team(unsigned char state)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_in_team>::From(_tbuf,pPlayer,state);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}

void 
gplayer_dispatcher::send_party_apply(int id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::team_apply_request>::From(_tbuf,id);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::query_info_1(int uid,int cs_index, int cs_sid)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<single_data_header>::From(_tbuf,PLAYER_INFO_1);
	if(CMD::Make<INFO::player_info_1>::From(_tbuf,(gplayer*)_imp->_parent))
	{
		send_ls_msg(cs_index,uid,cs_sid,_tbuf);
	}
}

void 
gplayer_dispatcher::concurrent_emote_request(int id, unsigned short action)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::concurrent_emote_request>::From(_tbuf,id,action);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::do_concurrent_emote(int id, unsigned short action)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::do_concurrent_emote>::From(_tbuf,pPlayer->ID.id, id,action);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::mafia_info_notify()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::mafia_info_notify>::From(_tbuf,pPlayer); 
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::mafia_trade_start()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::mafia_trade_start>::From(_tbuf); 
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::mafia_trade_end()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::mafia_trade_end>::From(_tbuf); 
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::task_deliver_item(int type, int expire_date, int amount, int slot_amount, int where,int index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::task_deliver_item>::From(_tbuf,type,expire_date,amount,slot_amount,where & 0xFF,index & 0xFF);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::task_deliver_reputaion(int offset,int cur_reputaion)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::task_deliver_reputaion>::From(_tbuf,offset,cur_reputaion);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::task_deliver_exp(int exp, int sp)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::task_deliver_exp>::From(_tbuf,exp,sp);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::task_deliver_money(size_t amount, size_t money)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::task_deliver_money>::From(_tbuf,amount,money);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::task_deliver_level2(int level2)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::task_deliver_level2>::From(_tbuf,pPlayer,level2);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::get_reputation(int reputation)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_reputation>::From(_tbuf,reputation);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::identify_result(char index, char result)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::identify_result>::From(_tbuf,index,result);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::change_shape(char shape)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_change_shape>::From(_tbuf,pPlayer, shape);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

//lgc
void gplayer_dispatcher::elf_refine_activate(char status)	//status 0-ȡ������ 1-����
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_elf_refine_activate>::From(_tbuf,pPlayer, status);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::enter_sanctuary()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_enter_sanctuary>::From(_tbuf, pPlayer->ID.id);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::leave_sanctuary()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_leave_sanctuary>::From(_tbuf, pPlayer->ID.id);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::begin_personal_market(int market_id, const char * name, size_t len)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_open_market>::From(_tbuf,pPlayer, market_id & 0xFF, name, len);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}

void 
gplayer_dispatcher::cancel_personal_market()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_cancel_market>::From(_tbuf,pPlayer);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::market_trade_success(int trader)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_market_trade_success>::From(_tbuf,trader);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::send_market_name(const XID & target, int cs_index, int sid,const char * name ,size_t len)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_market_name>::From(_tbuf,pPlayer,name,len);
	send_ls_msg(cs_index,target.id,sid,_tbuf);
}

void 
gplayer_dispatcher::player_start_travel(int  line_no,const A3DVECTOR & dest_pos,float speed,int vehicle)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_start_travel>::From(_tbuf,pPlayer,vehicle);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);

	_tbuf.clear();
	CMD::Make<CMD::self_start_travel>::From(_tbuf,speed,dest_pos,line_no,vehicle);
	send_ls_msg(pPlayer,_tbuf);

}

void 
gplayer_dispatcher::player_complete_travel(int vehicle)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_complete_travel>::From(_tbuf,pPlayer,vehicle);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::gm_toggle_invisible(char tmp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::gm_toggle_invisible>::From(_tbuf,tmp);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::toggle_invincible(char tmp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::gm_toggle_invincible>::From(_tbuf,tmp);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::trace_cur_pos(unsigned short seq)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::self_trace_cur_pos>::From(_tbuf,pPlayer->pos,seq);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::cast_instant_skill(const XID & target, int skill,unsigned char level)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_cast_instant_skill>::From(_tbuf,pPlayer,target,skill,level);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::cast_rune_instant_skill(const XID & target, int skill,unsigned char level)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_cast_rune_instant_skill>::From(_tbuf,pPlayer,target,skill,level);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::cast_elf_skill(const XID & target, int skill,unsigned char level)	//lgc
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_cast_elf_skill>::From(_tbuf,pPlayer,target,skill,level);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::activate_waypoint(unsigned short waypoint)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::activate_waypoint>::From(_tbuf,waypoint);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_waypoint_list(const unsigned short * list, size_t count)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_waypoint_list>::From(_tbuf,list,count);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::unlock_inventory_slot(unsigned char where, unsigned short index)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::unlock_inventory_slot>::From(_tbuf,where, index);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::team_invite_timeout(int who)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::team_invite_timeout>::From(_tbuf,who);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::enable_pvp_state(char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enable_pvp>::From(_tbuf,pPlayer,type);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::disable_pvp_state(char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_disable_pvp>::From(_tbuf,pPlayer,type);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_pvp_cooldown(int cool_down)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pvp_cooldown>::From(_tbuf,cool_down,PVP_STATE_COOLDOWN);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::send_cooldown_data()
{
	raw_wrapper rw;
	((gplayer_imp*)_imp)->GetCoolDownDataForClient(rw);
	
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::cooldown_data>::From(_tbuf,rw.data(),rw.size());
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::skill_ability_notify(int id, int ability)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::skill_ability_notify>::From(_tbuf,id, ability);
	send_ls_msg(pPlayer,_tbuf);
}


void
gplayer_dispatcher::personal_market_available()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::personal_market_available>::From(_tbuf);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::breath_data(int breath, int breath_capacity)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::breath_data>::From(_tbuf,breath, breath_capacity);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::stop_dive()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_stop_dive>::From(_tbuf);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::trade_away_item(int buyer,short inv_idx,int type, size_t count)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trade_away_item>::From(_tbuf,buyer, inv_idx,type, count);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_enable_fashion_mode(char is_enable)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enable_fashion_mode>::From(_tbuf,pPlayer, is_enable);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::player_enable_free_pvp(char is_enable)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::enable_free_pvp_mode>::From(_tbuf,is_enable);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_enable_effect(short effect)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enable_effect>::From(_tbuf,pPlayer->ID.id, effect);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::player_disable_effect(short effect)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_disable_effect>::From(_tbuf,pPlayer->ID.id, effect);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::enable_resurrect_state(float exp_reduce)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::enable_resurrect_state>::From(_tbuf,exp_reduce);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::object_is_invalid(int id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_is_invalid>::From(_tbuf,id);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::set_cooldown(int idx, int cooldown)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::set_cooldown>::From(_tbuf,idx, cooldown);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::change_team_leader(const XID & old_leader, const XID & new_leader)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::change_team_leader>::From(_tbuf,old_leader,new_leader);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::kickout_instance(char reason, int timeout)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::kickout_instance>::From(_tbuf,world_manager::GetWorldTag(), reason, timeout);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::begin_cosmetic(unsigned short inv_index)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_cosmetic_begin>::From(_tbuf,inv_index);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::end_cosmetic(unsigned short inv_index)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_cosmetic_end>::From(_tbuf,inv_index);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::cosmetic_success(unsigned short crc)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::cosmetic_success>::From(_tbuf,pPlayer->ID,crc);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::cast_pos_skill(const A3DVECTOR & pos, const XID &target, int skill,unsigned short time,unsigned char level)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_cast_pos_skill>::From(_tbuf,pPlayer->ID,pos,target,skill,time,level);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::change_move_seq(unsigned short seq)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::change_move_seq>::From(_tbuf,seq);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::server_config_data()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::server_config_data>::From(_tbuf,globaldata_getmalltimestamp(),globaldata_getmall2timestamp());
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::active_rush_mode(char is_active)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_rush_mode>::From(_tbuf,is_active);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::produce_null(int recipe_id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::produce_null>::From(_tbuf,recipe_id);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::trashbox_capacity_notify(int where, int cap)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::trashbox_capacity_notify>::From(_tbuf,where, cap);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::enable_double_exp_time(int mode, int end_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::double_exp_time>::From(_tbuf,mode, end_time);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::available_double_exp_time()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	gplayer_imp *pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::available_double_exp_time>::From(_tbuf,pImp->GetDoubleExpAvailTime());
	send_ls_msg(pPlayer,_tbuf);
}


void
gplayer_dispatcher::active_pvp_combat_state(bool is_active)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::active_pvp_combat_state>::From(_tbuf,pPlayer,is_active);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void
gplayer_dispatcher::duel_recv_request(const XID &target)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_recv_request>::From(_tbuf,target);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::duel_reject_request(const XID &target,int reason) 
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_reject_request>::From(_tbuf,target,reason);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::duel_prepare(const XID & target, int delay)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_prepare>::From(_tbuf,target,delay);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::duel_cancel(const XID & target) 
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_cancel>::From(_tbuf,target);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::duel_start(const XID & who) 
{
	_tbuf.clear();
	//֪ͨ�Լ�������ʼ
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_start>::From(_tbuf,who);
	send_ls_msg(pPlayer,_tbuf);

	//֪ͨ��Χ������ʼ
	_tbuf.clear();
	CMD::Make<CMD::else_duel_start>::From(_tbuf,pPlayer->ID);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}

void 
gplayer_dispatcher::duel_stop()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_stop>::From(_tbuf,pPlayer->ID);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::duel_result(const XID & target, bool is_failed) 
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::duel_result>::From(_tbuf,target, pPlayer->ID, is_failed);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_bind_request(const XID & target)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_bind_request>::From(_tbuf,target);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_bind_invite(const XID & target)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_bind_invite>::From(_tbuf,target);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_bind_request_reply(const XID & target, int param)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_bind_request_reply>::From(_tbuf,target,param);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_bind_invite_reply(const XID & target, int param)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_bind_invite_reply>::From(_tbuf,target,param);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_bind_start(const XID & target)
{
	//���startʼ���������Ǹ�������
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_bind_start>::From(_tbuf,target, pPlayer->ID);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_bind_stop()
{
	//���startʼ�������������Ǹ�������
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_bind_stop>::From(_tbuf,pPlayer->ID);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_mounting(int mount_id, unsigned short mount_color)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_mounting>::From(_tbuf,pPlayer->ID, mount_id, mount_color);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::send_equip_detail(int cs_index, int cs_sid,int target, const void * data, size_t size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_equip_detail>::From(_tbuf,pPlayer->ID, data,size);
	send_ls_msg(cs_index,target,cs_sid,_tbuf);
}

void 
gplayer_dispatcher::send_inventory_detail(int cs_index, int cs_sid, int target, size_t money, unsigned char inv_size, const void * data, size_t size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_inventory_detail>::From(_tbuf,pPlayer->ID, money, inv_size,data,size);
	send_ls_msg(cs_index,target,cs_sid,_tbuf);
}

void gplayer_dispatcher::send_others_property(const void * data, size_t size, const void * self_data, size_t self_size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_property>::From(_tbuf,data,size,self_data,self_size);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::pariah_duration(int remain_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pariah_duration>::From(_tbuf,remain_time);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::gain_pet(int index, const void * buf, size_t size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_gain_pet>::From(_tbuf,index, buf,size);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::free_pet(int index, int pet_id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_free_pet>::From(_tbuf,index, pet_id);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::summon_pet(int index, int pet_tid, int pet_id, int life_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_summon_pet>::From(_tbuf,index, pet_tid,pet_id,life_time);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::start_pet_operation(int index, int pet_id,int delay,int operation)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_start_pet_op>::From(_tbuf,index, pet_id,delay,operation);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::end_pet_operation()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_stop_pet_op>::From(_tbuf);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::recall_pet(int index, int pet_id, char reason)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_recall_pet>::From(_tbuf,index, pet_id, reason);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::pet_recv_exp(int index, int pet_id, int exp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pet_recv_exp>::From(_tbuf,index, pet_id,exp);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::pet_level_up(int index, int pet_id, int level, int cur_exp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pet_levelup>::From(_tbuf,index, pet_id,level,cur_exp);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::pet_room_capacity(int cap)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pet_room_capacity>::From(_tbuf,cap);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::notify_pet_honor(int index,int honor)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pet_honor_point>::From(_tbuf,index,honor);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::notify_pet_hunger(int index, int hunger)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pet_hunger_gauge>::From(_tbuf,index,hunger);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::send_pet_room(pet_data ** pData, size_t start, size_t end)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_pet_room>::From(_tbuf,pData,start, end);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::player_change_spouse(int id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_change_spouse>::From(_tbuf,pPlayer, id);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void gplayer_dispatcher::notify_safe_lock(char active, int time, int max_time)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::notify_safe_lock>::From( _tbuf, active, time + g_timer.get_systime(), max_time);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::enter_battleground(int role,int battle_id,int end_timestamp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::enter_battleground>::From(_tbuf,role,battle_id, end_timestamp);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::battle_result(int result)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::battle_result>::From(_tbuf,result);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::battle_score(int oscore, int ogoal, int dscore, int dgoal)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::battle_score>::From(_tbuf,oscore,ogoal,dscore,dgoal);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::pet_dead(int index)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_dead>::From(_tbuf,index);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::pet_revive(int index,float hp_factor)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_revive>::From(_tbuf,index,hp_factor);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::pet_hp_notify(int index, float hp_factor, int cur_hp, float mp_factor, int cur_mp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_hp_notify>::From(_tbuf,index,hp_factor,cur_hp,mp_factor,cur_mp);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::pet_ai_state(char aggro_state, char stay_state)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_ai_state>::From(_tbuf,aggro_state,stay_state);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::refine_result(int rst)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::refine_result>::From(_tbuf,rst);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::pet_set_cooldown(int index, int cd_index, int msec)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_set_cooldown>::From(_tbuf,index, cd_index, msec);
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::player_cash(int cash)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_cash>::From(_tbuf,cash);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::player_bind_success(size_t index, int id)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_bind_success>::From(_tbuf,index, id);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_change_inventory_size(int new_size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_change_inventory_size>::From(_tbuf,new_size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_pvp_mode()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_pvp_mode>::From(_tbuf,world_manager::GetWorldParam().pve_mode?0:1);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_wallow_info(int level, int ptime, int light_t, int heavy_t, int reason)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_wallow_info>::From(_tbuf, world_manager::AntiWallow()?1:0,level, ptime, light_t , heavy_t, reason);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::mall_item_buy_failed(short index, char reason)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::mall_item_buy_failed>::From(_tbuf, index, reason);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::equip_trashbox_item(int where, unsigned char trash_idx,unsigned char equip_idx) 
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::equip_trashbox_item>::From(_tbuf,where,trash_idx,equip_idx);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::security_passwd_checked()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::security_passwd_checked>::From(_tbuf);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::appear_to_spec(int invi_degree)
{
	on_dec_invisible(invi_degree,0);
}

void gplayer_dispatcher::disappear_to_spec(int invi_degree)
{
	on_inc_invisible(0,invi_degree);
}

void gplayer_dispatcher::on_inc_invisible(int prev_invi_degree, int cur_invi_degree)
{
	ASSERT(cur_invi_degree > prev_invi_degree);
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer *)_imp->_parent;
	CMD::Make<CMD::object_disappear>::From(_tbuf,pPlayer);
	slice * pPiece = pPlayer->pPiece;
	AutoBroadcastCSMsgToSpec(_imp->_plane,pPiece,_tbuf,cur_invi_degree,prev_invi_degree,pPlayer->team_id,pPlayer->ID.id);

	//��ʱ�Ȳ������ڱ߽����������
	/*if(pPiece->IsBorder())
	{
		extern_object_manager::SendDisappearMsg<0>(_imp->_plane,pPlayer,pPiece);
	}*/
}

void gplayer_dispatcher::on_dec_invisible(int prev_invi_degree, int cur_invi_degree)
{
	ASSERT(cur_invi_degree < prev_invi_degree);
	_tbuf.clear();
	using namespace S2C;
	gplayer *pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_enter_world>::From(_tbuf,pPlayer);
	slice * pPiece = pPlayer->pPiece;
	AutoBroadcastCSMsgToSpec(_imp->_plane,pPiece,_tbuf,prev_invi_degree,cur_invi_degree,pPlayer->team_id,pPlayer->ID.id);
}

void 
gplayer_dispatcher::on_inc_anti_invisible(int prev_a_invi_degree, int cur_a_invi_degree)
{
	ASSERT(cur_a_invi_degree > prev_a_invi_degree);	
	begin_transfer();
	//��ȡ�������ȼ���ߺ��ܶ࿴���Ķ���
	world * pPlane = _imp->_plane;
	slice * pPiece = _imp->_parent->pPiece;
	get_slice_info_visible(pPiece,_nw,_mw,_pw,cur_a_invi_degree,prev_a_invi_degree);

	int i;
	int total = pPlane->w_far_vision;
	int index = pPlane->GetGrid().GetSliceIndex(pPiece);
	int slice_x,slice_z;
	pPlane->GetGrid().Index2Pos(index,slice_x,slice_z);
	for(i = 0; i <total; i ++)
	{
		world::off_node_t &node = pPlane->w_off_list[i]; 
		int nx = slice_x + node.x_off; 
		int nz = slice_z + node.z_off; 
		if(nx < 0 || nz < 0 || nx >= pPlane->GetGrid().reg_column || nz >= pPlane->GetGrid().reg_row) continue;
		slice * pNewPiece = pPiece + node.idx_off;
		if(i <= pPlane->w_true_vision)
		{
			get_slice_info_visible(pNewPiece,_nw,_mw,_pw,cur_a_invi_degree,prev_a_invi_degree);
		}
		else
		{
			get_slice_info_visible(pNewPiece,_nw,_mw,_pw,cur_a_invi_degree,prev_a_invi_degree);
		}
		wrapper_test<MIN_SEND_COUNT>(_pw,_header,S2C::PLAYER_INFO_1_LIST);
		wrapper_test<MIN_SEND_COUNT>(_mw,_header,S2C::MATTER_INFO_LIST);
		wrapper_test<MIN_SEND_COUNT>(_nw,_header,S2C::NPC_INFO_LIST);
	}

	end_transfer();
	return ;
}

void 
gplayer_dispatcher::on_dec_anti_invisible(int prev_a_invi_degree, int cur_a_invi_degree)
{
	ASSERT(cur_a_invi_degree < prev_a_invi_degree);	
	begin_transfer();
	//��ȡ�������ȼ����ͺ���Ҳ�������Ķ���
	world * pPlane = _imp->_plane;
	slice * pPiece = _imp->_parent->pPiece;
	int team_id = ((gplayer*)_imp->_parent)->team_id;
	gather_slice_object_visible(pPiece,_leave_list,prev_a_invi_degree,cur_a_invi_degree,team_id);

	int i;
	int total = pPlane->w_far_vision;
	int index = pPlane->GetGrid().GetSliceIndex(pPiece);
	int slice_x,slice_z;
	pPlane->GetGrid().Index2Pos(index,slice_x,slice_z);
	for(i = 0; i <total; i ++)
	{
		world::off_node_t &node = pPlane->w_off_list[i]; 
		int nx = slice_x + node.x_off; 
		int nz = slice_z + node.z_off; 
		if(nx < 0 || nz < 0 || nx >= pPlane->GetGrid().reg_column || nz >= pPlane->GetGrid().reg_row) continue;
		slice * pNewPiece = pPiece + node.idx_off;
		if(i <= pPlane->w_true_vision)
		{
			gather_slice_object_visible(pNewPiece,_leave_list,prev_a_invi_degree,cur_a_invi_degree,team_id);
		}
		else
		{
			gather_slice_object_visible(pNewPiece,_leave_list,prev_a_invi_degree,cur_a_invi_degree,team_id);
		}
	}

	end_transfer();
	return ;
}

void 
gplayer_dispatcher::hp_steal(int hp)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::hp_steal>::From(_tbuf,hp);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::player_dividend(int dividend)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_dividend>::From(_tbuf,dividend);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::dividend_mall_item_buy_failed(short index, char reason)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::dividend_mall_item_buy_failed>::From(_tbuf, index, reason);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::multi_exp_info(int last_timestamp, float enhance_factor)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::multi_exp_info>::From(_tbuf, last_timestamp, enhance_factor);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::change_multi_exp_state(char state, int enchance_time, int buffer_time, int impair_time, int activate_times_left)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::change_multi_exp_state>::From(_tbuf, state, enchance_time, buffer_time, impair_time, activate_times_left);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::send_world_life_time(int life_time)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::world_life_time>::From(_tbuf, life_time);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::wedding_book_success(int type)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::wedding_book_success>::From(_tbuf,type);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::calc_network_delay(int timestamp)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::calc_network_delay>::From(_tbuf,timestamp);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_knockback(const A3DVECTOR & pos, int time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_knockback>::From(_tbuf,pPlayer->ID.id,pos,time);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf);
}

void
gplayer_dispatcher::summon_plant_pet(int plant_tid, int plant_id, int life_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_summon_plant_pet>::From(_tbuf,plant_tid,plant_id,life_time);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::plant_pet_disappear(int id, char reason)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::plant_pet_disappear>::From(_tbuf,id,reason);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::plant_pet_hp_notify(int id, float hp_factor, int cur_hp, float mp_factor, int cur_mp)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::plant_pet_hp_notify>::From(_tbuf,id,hp_factor,cur_hp,mp_factor,cur_mp);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::pet_property(int index, const extend_prop & prop)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_property>::From(_tbuf,index,prop);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::faction_contrib_notify()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::faction_contrib_notify>::From(_tbuf,pImp->_faction_contrib.consume_contrib,pImp->_faction_contrib.exp_contrib,pImp->_faction_contrib.cumulate_contrib);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::faction_relation_notify()
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	gplayer_imp * pImp = (gplayer_imp *)_imp;
	CMD::Make<CMD::faction_relation_notify>::From(_tbuf);
	_tbuf << pImp->_faction_alliance.size();
	for(abase::hash_map<int,int>::iterator it=pImp->_faction_alliance.begin(); it!=pImp->_faction_alliance.end(); ++it)
		_tbuf << it->first;
	_tbuf << pImp->_faction_hostile.size();
	for(abase::hash_map<int,int>::iterator it=pImp->_faction_hostile.begin(); it!=pImp->_faction_hostile.end(); ++it)
		_tbuf << it->first;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::enter_factionfortress(int role_in_war, int factionid, int offense_endtime)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::enter_factionfortress>::From(_tbuf,role_in_war,factionid,offense_endtime);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_equip_disabled(int64_t mask) 
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_equip_disabled>::From(_tbuf, pPlayer->ID, mask);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::send_spec_item_list(int cs_index, int cs_sid, int target, int type, void * data, size_t size)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_spec_item_list>::From(_tbuf,pPlayer->ID,type,data,size);
	send_ls_msg(cs_index,target,cs_sid,_tbuf);
}

void 
gplayer_dispatcher::send_error_message(int cs_index, int cs_sid, int target, int message)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::error_msg>::From(_tbuf,message);
	send_ls_msg(cs_index,target,cs_sid,_tbuf);
}

void 
gplayer_dispatcher::congregate_request(char type, int sponsor, int timeout)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::congregate_request>::From(_tbuf,type,sponsor,timeout);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::reject_congregate(char type, int id)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::reject_congregate>::From(_tbuf,type,id);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::congregate_start(char type, int time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::congregate_start>::From(_tbuf,type,pPlayer->ID,time);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::cancel_congregate(char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::cancel_congregate>::From(_tbuf,type,pPlayer->ID);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::engrave_start(int recipe_id, int use_time)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::engrave_start>::From(_tbuf,recipe_id,use_time);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::engrave_end()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::engrave_end>::From(_tbuf);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::engrave_result(int addon_num)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::engrave_result>::From(_tbuf,addon_num);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::addonregen_start(int recipe_id, int use_time)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::addonregen_start>::From(_tbuf,recipe_id,use_time);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::addonregen_end()
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::addonregen_end>::From(_tbuf);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::addonregen_result(int addon_num)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::addonregen_result>::From(_tbuf,addon_num);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::invisible_obj_list(gobject ** ppObject, size_t count)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::invisible_obj_list>::From(_tbuf,_imp->_parent->ID.id,ppObject,count);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::set_player_limit(int index, char b)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::set_player_limit>::From(_tbuf,index,b);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_teleport(const A3DVECTOR& pos, unsigned short use_time, char mode)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_teleport>::From(_tbuf,pPlayer->ID.id,pos,use_time,mode);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::forbid_be_selected(char b)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::object_forbid_be_selected>::From(_tbuf,pPlayer->ID.id,b);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::send_player_force_data(int cur_force, size_t count, const void * data, size_t data_size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_force_data>::From(_tbuf,cur_force,count,data,data_size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_force_changed(int force)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_force_changed>::From(_tbuf,pPlayer->ID.id,force);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_force_data_update(int force, int repu, int contri)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_force_data_update>::From(_tbuf,force,repu,contri);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::send_force_global_data(char data_ready, size_t count, const void * data, size_t data_size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::force_global_data>::From(_tbuf,data_ready,count,data,data_size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::add_multiobj_effect(int target, char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::add_multiobj_effect>::From(_tbuf,pPlayer->ID.id,target,type);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::remove_multiobj_effect(int target, char type)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::remove_multiobj_effect>::From(_tbuf,pPlayer->ID.id,target,type);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::enter_wedding_scene(int groom, int bride)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::enter_wedding_scene>::From(_tbuf,groom,bride);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::produce4_item_info(int stime, item_data & data, unsigned short crc)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::produce4_item_info>::From(_tbuf,stime, data, crc);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::online_award_data(int total_award_time, size_t count, const void * data, size_t data_size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::online_award_data>::From(_tbuf,total_award_time,count,data,data_size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::toggle_online_award(int type, char activate)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::toggle_online_award>::From(_tbuf,type,activate);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::update_profit_time(char flag, int profit_time, int profit_level)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	char profit_map = 0;
	if(world_manager::ProfitTimeLimit2()) profit_map = 0x02;
	else if(world_manager::ProfitTimeLimit()) profit_map = 0x01;
	CMD::Make<CMD::player_profit_time>::From(_tbuf, flag, (char)profit_map, profit_time, profit_level);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::notify_profit_state(char state)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::set_profittime_state>::From(_tbuf, state);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::enter_nonpenalty_pvp_state(char state)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::enter_nonpenalty_pvp_state>::From(_tbuf, state);
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::self_country_notify(int country_id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::self_country_notify>::From(_tbuf, country_id);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::player_country_changed(int country_id)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_country_changed>::From(_tbuf,pPlayer->ID,country_id);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::enter_countrybattle(int role, int battle_id,int end_time,int offense_country, int defence_country)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::enter_countrybattle>::From(_tbuf,role,battle_id, end_time,offense_country, defence_country);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::countrybattle_result(int result)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::countrybattle_result>::From(_tbuf,result);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::countrybattle_score(int oscore, int ogoal, int dscore, int dgoal)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::countrybattle_score>::From(_tbuf,oscore,ogoal,dscore,dgoal);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::countrybattle_resurrect_rest_times(int times)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::countrybattle_resurrect_rest_times>::From(_tbuf,times);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::countrybattle_became_flag_carrier(char is_carrier)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::countrybattle_became_flag_carrier>::From(_tbuf,is_carrier);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::countrybattle_personal_score(int combat_time, int attend_time, int kill_count, int death_count, int country_kill_count, int country_death_count)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::countrybattle_personal_score>::From(_tbuf,combat_time,attend_time,kill_count,death_count,country_kill_count,country_death_count);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::defense_rune_enabled(char rune_type, char enable)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::defense_rune_enabled>::From(_tbuf,rune_type,enable);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::countrybattle_info(int attacker_count, int defender_count)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::countrybattle_info>::From(_tbuf,attacker_count,defender_count);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::cash_money_exchg_rate(char open, int rate)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::cash_money_exchg_rate>::From(_tbuf,open,rate);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::pet_rebuild_inherit_start(size_t index,int use_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_rebuild_inherit_start>::From(_tbuf,index,use_time);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::pet_rebuild_inherit_info(int stime,int pet_id,size_t index,int r_attack,int r_defense,int r_hp,int r_atk_lvl,int r_def_lvl)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_rebuild_inherit_info>::From(_tbuf,stime,pet_id,index,r_attack,r_defense,r_hp,r_atk_lvl,r_def_lvl);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::pet_rebuild_inherit_end(size_t index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::pet_rebuild_inherit_end>::From(_tbuf,index);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::pet_evolution_done(size_t index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::pet_evolution_done>::From(_tbuf,index);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::pet_rebuild_nature_start(size_t index,int use_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_rebuild_nature_start>::From(_tbuf,index,use_time);
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::pet_rebuild_nature_info(int stime,int pet_id,size_t index,int nature)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer *pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::pet_rebuild_nature_info>::From(_tbuf,stime,pet_id,index,nature);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::pet_rebuild_nature_end(size_t index)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::pet_rebuild_nature_end>::From(_tbuf,index);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::notify_meridian_data(int meridian_level,int lifegate_times,int deathgate_times,int free_refine_times,int paid_refine_times,int continu_login_days,int trigrams_map[3])
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::notify_meridian_data>::From(_tbuf,meridian_level,lifegate_times,deathgate_times,free_refine_times,paid_refine_times,continu_login_days,trigrams_map);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::try_refine_meridian_result(int index,int result)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::try_refine_meridian_result>::From(_tbuf,index,result);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::equip_addon_update_notify(unsigned char update_type,unsigned char equip_idx,unsigned char equip_socket_idx,int old_stone_type,int new_stone_type)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::equip_addon_update_notify>::From(_tbuf,update_type,equip_idx,equip_socket_idx,old_stone_type,new_stone_type);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void
gplayer_dispatcher::self_king_notify(char is_king, int expire_time)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::self_king_notify>::From(_tbuf, is_king, expire_time);
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_dispatcher::player_king_changed(char is_king)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_king_changed>::From(_tbuf,pPlayer->ID.id,is_king);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::notify_touch_query(int64_t income,int64_t remain,int retcode)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::query_touch_point>::From(_tbuf,income,remain,retcode);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::notify_addup_money(int64_t addupmoney)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::query_addup_money>::From(_tbuf,addupmoney);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::notify_touch_cost(int64_t income,int64_t remain,unsigned int cost,unsigned int index,unsigned int lots,int retcode)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::cost_touch_point>::From(_tbuf,income,remain,cost,index,lots,retcode);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::query_title(int roleid,int count,int ecount, const void * data, size_t data_size, const void *edata, size_t edata_size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::query_title>::From(_tbuf,roleid,count,ecount,data,data_size,edata,edata_size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::notify_curr_title_change(int roleid,unsigned short titleid)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::change_title>::From(_tbuf,roleid,titleid);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);

}

void 
gplayer_dispatcher::notify_title_modify(unsigned short titleid,int expiretime,char flag)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::notify_title_modify>::From(_tbuf,titleid,expiretime,flag);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);

}

void 
gplayer_dispatcher::refresh_signin(char type,int moncal,int cys,int lys, int uptime, int localtime, char awardedtimes, char latesignintimes)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::refresh_signin>::From(_tbuf,type,moncal,cys,lys,uptime,localtime, awardedtimes, latesignintimes);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::notify_giftcard_redeem(int retcode,int cardtype,int parenttype, const char(&cardnumber)[player_giftcard::GIFT_CARDNUMBER_LEN] )
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::notify_giftcard_redeem>::From(_tbuf,retcode, cardtype, parenttype, cardnumber);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_reincarnation(size_t reincarnation_times)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_reincarnation>::From(_tbuf,pPlayer->ID.id,(int)reincarnation_times);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::activate_reincarnation_tome(char active)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::activate_reincarnation_tome>::From(_tbuf,active);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::enter_trickbattle(int role, int battle_id,int end_time)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::enter_trickbattle>::From(_tbuf,role,battle_id,end_time);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::trickbattle_personal_score(int kill, int death, int score, int multi_kill)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::trickbattle_personal_score>::From(_tbuf,kill, death, score, multi_kill);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::trickbattle_chariot_info(int chariot, int energy)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::trickbattle_chariot_info>::From(_tbuf,chariot,energy);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::realm_exp_receive(int exp,int receive_exp)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::realm_exp>::From(_tbuf,exp,receive_exp);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void gplayer_dispatcher::realm_level_up(unsigned char level)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::realm_level_up>::From(_tbuf,pPlayer->ID.id,level);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void 
gplayer_dispatcher::player_leadership(int leadership, int inc_leadership)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_leadership>::From(_tbuf,leadership,inc_leadership);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_world_contribution(int world_contrib, int change, int total_cost)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::player_world_contribution>::From(_tbuf,world_contrib, change, total_cost);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::send_scene_service_npc_list(unsigned int count, int * data)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::scene_service_npc_list>::From(_tbuf, count, data);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_screen_effect_notify(int type ,int eid, int state)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::client_screen_effect>::From(_tbuf, type, eid, state);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_combo_skill_prepare(int skillid,int timestamp,int arg1, int arg2, int arg3)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::combo_skill_prepare>::From(_tbuf,skillid,timestamp,arg1,arg2,arg3);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_pray_distance_change(float pd)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::pray_distance_change>::From(_tbuf,pd);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::instance_reenter_notify(int tag, int timeout)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::instance_reenter_notify>::From(_tbuf,tag,timeout);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::generalcard_collection_data(const void * buf, size_t size)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::generalcard_collection_data>::From(_tbuf,buf,size);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::add_generalcard_collection(size_t collection_idx)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::add_generalcard_collection>::From(_tbuf,collection_idx);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void
gplayer_dispatcher::player_active_combat(bool is_active)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_active_combat>::From(_tbuf,pPlayer,is_active);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,pPlayer->ID.id);
}

void
gplayer_dispatcher::randommap_order_init(int row,int col,const int* room_src)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::random_map_order>::From(_tbuf,world_manager::GetWorldTag(),row,col,room_src);
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::random_mall_shopping_result(int eid,int opt,int res,int itemid,int price,bool flag)
{
	_tbuf.clear();
	using namespace S2C;
	CMD::Make<CMD::random_mall_shopping_result>::From(_tbuf,eid,opt,res,itemid,price,flag);
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer,_tbuf);
}

void 
gplayer_dispatcher::player_mafia_pvp_mask_notify(unsigned char mafia_pvp_mask)
{
	_tbuf.clear();
	using namespace S2C;
	gplayer * pPlayer = (gplayer*)_imp->_parent;
	CMD::Make<CMD::player_mafia_pvp_mask_notify>::From(_tbuf,pPlayer->ID.id,mafia_pvp_mask);
	AutoBroadcastCSMsg(_imp->_plane,pPlayer->pPiece,_tbuf,-1);
}

void gplayer_dispatcher::equip_can_inherit_addons(int equip_id, unsigned char inv_idx, unsigned char addon_num, int addon_id_list[])
{    
    _tbuf.clear();
    using namespace S2C;
	CMD::Make<CMD::equip_can_inherit_addons>::From(_tbuf, equip_id, inv_idx, addon_num, addon_id_list);
	gplayer* pPlayer = (gplayer*)_imp->_parent;
	send_ls_msg(pPlayer, _tbuf);
}

void 
gplayer_imp::OnDeath(const XID & lastattack,bool is_pariah, char attacker_mode, int taskdead)
{
	__PRINTF("player %d dead \n",_parent->ID.id);
	if((gplayer*)_parent->IsZombie())
	{
		//�Ѿ���������Ӧ���ٴε�����
		return ;
	}
	if(_player_state == PLAYER_SIT_DOWN)
	{
		LeaveStayInState();
		_player_state = PLAYER_STATE_NORMAL;
	}
	else if(_player_state == PLAYER_STATE_MARKET)
	{
		CancelPersonalMarket();
	}
	else if(_player_state == PLAYER_STATE_BIND)
	{
		//�����״̬ 
		_bind_player.PlayerCancel(this);
	}
	
	
	ClearSession();
	ClearAction();
	ActiveCombatState(false);


	bool bCanDrop = _player_state == PLAYER_STATE_NORMAL ||
			_player_state == PLAYER_DISCONNECT ||
			_player_state == PLAYER_SIT_DOWN ||
			_player_state == PLAYER_STATE_BIND;

	bool free_resurrect = false;
	if(!_free_pvp_mode && _basic.level > LOW_PROTECT_LEVEL && taskdead != 2 && !_nonpenalty_pvp_state && !world_manager::GetWorldFlag().nonpenalty_pvp_flag)
	{
		//����������޵Ĵ��ڣ�������ڣ������κ���ʧ����ɾ��һ����������
		int dummy_id = ITEM_POPPET_DUMMY_ID2;
		int dummy_index = _inventory.Find(0, ITEM_POPPET_DUMMY_ID2);
		if(dummy_index < 0)
		{
			dummy_id = ITEM_POPPET_DUMMY_ID;
			dummy_index = _inventory.Find(0, ITEM_POPPET_DUMMY_ID);
			if(dummy_index < 0)
			{
				dummy_id = ITEM_POPPET_DUMMY_ID3;
				dummy_index = _inventory.Find(0, ITEM_POPPET_DUMMY_ID3);
			}
		}
		if(dummy_index >= 0)
		{
			//����
			if(bCanDrop)
			{
				free_resurrect = true;

				item& it = _inventory[dummy_index];
				UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
				
				_inventory.DecAmount(dummy_index, 1);
				_runner->player_drop_item(IL_INVENTORY,dummy_index,dummy_id,1,S2C::DROP_TYPE_USE);
			}
		}
	}

	if(bCanDrop && !free_resurrect)
	{
		if(!_free_pvp_mode && _basic.level > LOW_PROTECT_LEVEL && taskdead != 2 && !_nonpenalty_pvp_state && !world_manager::GetWorldFlag().nonpenalty_pvp_flag)
		{
			//������PVP�ͷǵͼ�������ʱ��Ų�������Ʒ
			if(_pvp_enable_flag)
			{
				//��PK���غ���PK���غ��״̬������
				_invade_ctrl.OnDeath(lastattack);
			}		
			else
			{
				/*//�����չ̶���ʽ�����
				//Ŀǰ������ɱ������ĸ���ͬ����
				float * pPropInv = NULL;
				pPropInv = player_template::GetMobNormalInventoryDropRate();
				int drop_count1 = abase::RandSelect(pPropInv,10);
				if(drop_count1 > 0)
				{
					DropItemOnDeath(drop_count1,0);
				}*/
			}
		}
	}

	if(bCanDrop)
	{
		//����ض��������Ʒ
		ThrowDeadDropItem();
	}

	//�����ٻس���
	_petman.RecallPet(this);

	_basic.mp = 0;
	_basic.ap = 0;

	if(lastattack.type == GM_TYPE_PLAYER && is_pariah && _invader_state == INVADER_LVL_0 && !_free_pvp_mode)
	{
		//�Ǻ��������ͺ�����Ϣ
		SendTo<0>(GM_MSG_PLAYER_BECOME_PARIAH,lastattack,0);
	}

	if(lastattack.type == GM_TYPE_NPC && taskdead == 0)
	{
		//��NPCɱ��������һ����Ϣ
		SendTo<0>(GM_MSG_PLAYER_KILLED_BY_NPC,lastattack,0);
	}

	if(lastattack.type == GM_TYPE_PLAYER && (attacker_mode & attack_msg::PVP_FEEDBACK_KILL))
	{
		msg_player_killed_info_t data;
		GetPlayerClass(data.cls, data.gender);
		data.level 		= _basic.level;
		data.force_id 	= _player_force.GetForce();
		SendTo<0>(GM_MSG_PLAYER_KILLED_BY_PLAYER, lastattack, 0, &data, sizeof(data));
	}

	//�����Ĳ���������Zombie״̬
	_parent->b_zombie = true;
	if(lastattack.IsPlayerClass()) _kill_by_player = true; else _kill_by_player = false;

	//ע���ھ����������߸�����ܻ��в�Ӧ���еľ�����ʧ ���������Ǳ����ɱ���� ���Ի��ǲ����о�����ʧ��
	if(_free_pvp_mode || free_resurrect || taskdead == 2 || _nonpenalty_pvp_state || world_manager::GetWorldFlag().nonpenalty_pvp_flag)
	{
		//ֻҪ������PVPģʽ������Ϊ�Ǳ����ɱ�������������и������ʧ
		_kill_by_player = true;
	}

	_resurrect_state = false;
	_resurrect_exp_reduce = 1.0f;
	_resurrect_hp_factor = 0.f;
	_resurrect_mp_factor = 0.f;

	//����������Ϣ
	_runner->on_death(lastattack, false);
	int death_type = lastattack.type&0xFF;
	if(is_pariah) death_type |= 0x100;
	if(taskdead) death_type |= taskdead<<12;
	GLog::die(_parent->ID.id,death_type,lastattack.id);
	GLog::log(GLOG_INFO,"�û�%d��%dɱ�������(%d)",_parent->ID.id,lastattack.id,death_type);

	//��������Ĵ���
	PlayerTaskInterface task_if(this);
	OnTaskPlayerKilled(&task_if);

	//������������
	if(!world_manager::GetInstance()->IsUniqueWorld()) //����������
	{
		_plane->ModifyCommonValue(COMMON_VALUE_ID_PLAYERDEAD_COUNT, 1);
	}
	//�����������Զ���ʱ������session
//	session_player_reborn * pSession = new session_player_reborn(this);
//	pSession->SetDelay(50*10);
	// ����PVP ����
	if(((gplayer*)_parent)->mafia_pvp_mask && lastattack.IsPlayerClass())
	{
		MSG msg;
		msg_mafia_pvp_award_t mfa = { OI_GetMafiaID(), city_region::GetDomainID(_parent->pos.x,_parent->pos.z)};
		BuildMessage(msg, GM_MSG_MAFIA_PVP_AWARD, lastattack, _parent->ID, _parent->pos, AWARD_MAFIAPVP_HIJACK_KILL , &mfa, sizeof(mfa));
		_plane->PostLazyMessage(msg);
	}
}

int 
gplayer_imp::PlayerGetItemInfo(int where, int index)
{
	int rst;
	unsigned short crc = 0;
	item_data data;
	switch(where)
	{       
		case IL_INVENTORY:
			rst = _inventory.GetItemData(index,data,crc);
			break;
		case IL_EQUIPMENT:
			rst = _equipment.GetItemData(index,data,crc);
			break;
		case IL_TASK_INVENTORY:
			rst = _task_inventory.GetItemData(index,data,crc);
			break;
		case IL_TRASH_BOX:		//lgc
			rst = _trashbox.GetBackpack1().GetItemData(index,data,crc);
			break;
		case IL_USER_TRASH_BOX:
			rst = _user_trashbox.GetBackpack1().GetItemData(index,data,crc);
			break;
		default:
			rst = -1;
	}
	if(rst == 0)
	{
		//������
		_runner->self_item_empty_info(where,index);
	}else if(rst > 0)
	{
		//�ҵ���
		_runner->self_item_info(where,index,data,crc);
	}

	return rst;
}

int 
gplayer_imp::PlayerGetItemInfoList(int where, size_t count , unsigned char *list)
{
	item_list &inv = GetInventory(where);

	size_t cap = inv.Size();
	for(size_t i = 0; i < count; i ++)
	{
		size_t index = list[i];
		if(index < cap)
		{

			unsigned short crc = 0;
			item_data data;
			if(inv.GetItemData(index,data,crc) > 0)
			{
				_runner->self_item_info(where,index,data,crc);
			}
		}
	}
	

	return 0;
}

void 
gplayer_imp::TrashBoxOpen(bool view_only)
{
	if(view_only)
	{
		_trash_box_open_view_only_flag = true;
	}
	else
	{
		_trash_box_open_flag = true;
		IncTrashBoxChangeCounter();//��ʱ����Ϊ��Ʒ�䷢���˱仯
	}
	_runner->trashbox_open(0);
	GLog::log(GLOG_INFO,"�û�%d�����˲ֿ�",_parent->ID.id);
}

void 
gplayer_imp::TrashBoxClose(bool view_only)
{
	if(view_only)
	{
		_trash_box_open_view_only_flag = false;
	}
	else
	{
		_trash_box_open_flag = false;
	}
	_runner->trashbox_close(0);
	GLog::log(GLOG_INFO,"�û�%d�ر��˲ֿ�",_parent->ID.id);
}

void 
gplayer_imp::UserTrashBoxOpen()
{
	_user_trash_box_open_flag = true;
	IncUserTrashBoxChangeCounter();//��ʱ����Ϊ��Ʒ�䷢���˱仯
	_runner->trashbox_open(1);
	GLog::log(GLOG_INFO,"�û�%d�������ʺŲֿ�",_parent->ID.id);
}

void 
gplayer_imp::UserTrashBoxClose()
{
	_user_trash_box_open_flag = false;
	_runner->trashbox_close(1);
	GLog::log(GLOG_INFO,"�û�%d�ر����ʺŲֿ�",_parent->ID.id);
}

void
gplayer_imp::PlayerGetTrashBoxInfo(bool detail)
{
	if(detail)
	{
		raw_wrapper rw(1024);
		_trashbox.GetBackpack1().DetailSave(rw);
		_runner->self_inventory_detail_data(IL_TRASH_BOX,_trashbox.GetBackpack1().Size(),rw.data(),rw.size());

		rw.clear();
		_trashbox.GetBackpack2().DetailSave(rw);
		_runner->self_inventory_detail_data(IL_TRASH_BOX2,_trashbox.GetBackpack2().Size(),rw.data(),rw.size());
	}
	else
	{
		abase::octets buf;
		_trashbox.GetBackpack1().SimpleSave(buf);
		_runner->self_inventory_data(IL_TRASH_BOX, _trashbox.GetBackpack1().Size() , buf.begin(), buf.size());

		buf.clear();
		_trashbox.GetBackpack2().SimpleSave(buf);
		_runner->self_inventory_data(IL_TRASH_BOX2, _trashbox.GetBackpack2().Size() , buf.begin(), buf.size());

	}
	_runner->trashbox_wealth(0,_trashbox.GetMoney());
}

void
gplayer_imp::PlayerGetPortableTrashBoxInfo(bool detail)
{
	if(detail)
	{
		raw_wrapper rw(1024);
		_trashbox.GetBackpack3().DetailSave(rw);
		_runner->self_inventory_detail_data(IL_TRASH_BOX3,_trashbox.GetBackpack3().Size(),rw.data(),rw.size());

		rw.clear();
		_trashbox.GetBackpack4().DetailSave(rw);
		_runner->self_inventory_detail_data(IL_TRASH_BOX4,_trashbox.GetBackpack4().Size(),rw.data(),rw.size());
	}
	else
	{
		abase::octets buf;
		_trashbox.GetBackpack3().SimpleSave(buf);
		_runner->self_inventory_data(IL_TRASH_BOX3, _trashbox.GetBackpack3().Size() , buf.begin(), buf.size());
		
		buf.clear();
		_trashbox.GetBackpack4().SimpleSave(buf);
		_runner->self_inventory_data(IL_TRASH_BOX4, _trashbox.GetBackpack4().Size() , buf.begin(), buf.size());
	}
}

void
gplayer_imp::PlayerGetUserTrashBoxInfo(bool detail)
{
	if(detail)
	{
		raw_wrapper rw(1024);
		_user_trashbox.GetBackpack1().DetailSave(rw);
		_runner->self_inventory_detail_data(IL_USER_TRASH_BOX,_user_trashbox.GetBackpack1().Size(),rw.data(),rw.size());
	}
	else
	{
		abase::octets buf;
		_user_trashbox.GetBackpack1().SimpleSave(buf);
		_runner->self_inventory_data(IL_USER_TRASH_BOX, _user_trashbox.GetBackpack1().Size() , buf.begin(), buf.size());
	}
	_runner->trashbox_wealth(1,_user_trashbox.GetMoney());
}

void 
gplayer_imp::PlayerGetInventoryDetail(int where)
{
	raw_wrapper rw;
	unsigned char size;
	switch(where)
	{
		case IL_INVENTORY:
			_inventory.DetailSave(rw);
			size = _inventory.Size();
			break;
		case IL_EQUIPMENT:
			_equipment.DetailSave(rw);
			size = _equipment.Size();
			break;
		case IL_TASK_INVENTORY:
			_task_inventory.DetailSave(rw);
			size = _task_inventory.Size();
			break;
		default:
		ASSERT(false);
		return;
	}
	_runner->self_inventory_detail_data(where,size, rw.data(),rw.size());
}

void
gplayer_imp::PlayerExchangeInvItem(size_t idx1,size_t idx2)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}
	if(!_inventory.ExchangeItem(idx1,idx2))
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return ;
	}
	_runner->exchange_inventory_item(idx1,idx2);
}

void 
gplayer_imp::PlayerMoveInvItem(size_t src,size_t dest,size_t count)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}
	if(!_inventory.MoveItem(src,dest,&count))
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return ;
	}
	_runner->move_inventory_item(src,dest,count);
}

void
gplayer_imp::PlayerExchangeTrashItem(int where, size_t idx1,size_t idx2)
{
	item_list & box = GetTrashInventory(where);
	if(!box.ExchangeItem(idx1,idx2))
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return ;
	}
	//�����ֿ���޸Ĳ���Ҫ�򿪲ֿ⣬��Ҫ���Ӹı����
	if(IsPortableTrashBox(where)) IncTrashBoxChangeCounter();
	_runner->exchange_trashbox_item(where, idx1,idx2);
}

void 
gplayer_imp::PlayerMoveTrashItem(int where, size_t src,size_t dest,size_t count)
{
	item_list & box = GetTrashInventory(where);
	if(!box.MoveItem(src,dest,&count))
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return ;
	}
	//�����ֿ���޸Ĳ���Ҫ�򿪲ֿ⣬��Ҫ���Ӹı����
	if(IsPortableTrashBox(where)) IncTrashBoxChangeCounter();
	_runner->move_trashbox_item(where,src,dest,count);
}

static bool VerifySpecTrashBox(int where, item_list & inv, size_t idx)
{
	if(idx >= inv.Size()) return false;
	int id = inv[idx].type;
	if(id <= 0 ) return true;
	if(where == gplayer_imp::IL_TRASH_BOX2)
	{
		DATA_TYPE dt = world_manager::GetDataMan().get_data_type(id, ID_SPACE_ESSENCE);
		return dt== DT_MATERIAL_ESSENCE || dt == DT_TASKNORMALMATTER_ESSENCE || dt == DT_SKILLTOME_ESSENCE;
	}
	else if(where == gplayer_imp::IL_TRASH_BOX3)
	{
		return (world_manager::GetDataMan().get_data_type(id, ID_SPACE_ESSENCE) == DT_FASHION_ESSENCE);
	}
	else if(where == gplayer_imp::IL_TRASH_BOX4)
	{
		DATA_TYPE dt = world_manager::GetDataMan().get_data_type(id, ID_SPACE_ESSENCE);
		return dt == DT_POKER_ESSENCE || dt == DT_POKER_DICE_ESSENCE;
	}
	else if(where == gplayer_imp::IL_USER_TRASH_BOX)
	{
		if(inv[idx].proc_type & item::ITEM_PROC_TYPE_NOPUTIN_USERTRASH) return false;		
		if(world_manager::IsNoPutInUserTrashItem(id)) return false;
	}
	return true;
}

void 
gplayer_imp::PlayerExchangeTrashInv(int where, size_t idx_tra, size_t idx_inv)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}
	if(!VerifySpecTrashBox(where, _inventory, idx_inv))
	{
		return ;
	}

	item_list & box = GetTrashInventory(where);
	if(idx_tra >= box.Size() || idx_inv >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return ;
	}
	//�����ֿ�ȡ����ƷʱҪ�ܰ�ȫ��������
	if(IsPortableTrashBox(where) && box[idx_tra].type > 0 && !_security_passwd_checked)
	{
		_runner->error_message(S2C::ERR_SECURITY_PASSWD_UNCHECKED);
		return;
	}
	//�ʺŲֿ���Ʒ�����ɲ�����������������Ʒ
	if(where == gplayer_imp::IL_USER_TRASH_BOX && box.GetItemCount() >= box.Size()/2)
	{
		if(_inventory[idx_inv].type != -1 && box[idx_tra].type == -1)	return;
	}
	item it;
	box.Exchange(idx_tra,it);
	_inventory.Exchange(idx_inv,it);
	box.Exchange(idx_tra,it);
	//�����ֿ���޸Ĳ���Ҫ�򿪲ֿ⣬��Ҫ���Ӹı����
	if(IsPortableTrashBox(where)) IncTrashBoxChangeCounter();
	_runner->exchange_trashbox_inventory(where, idx_tra,idx_inv);

	//���ʺŲֿ��Ƴ�������Ʒ��Ҫ��¼log
	if(where == gplayer_imp::IL_USER_TRASH_BOX)
	{
		if(_inventory[idx_inv].type > 0)
			GLog::log(GLOG_INFO, "�û�%d��%d����Ʒ%d�Ƴ��ʺŲֿ�", _parent->ID.id, _inventory[idx_inv].count, _inventory[idx_inv].type);	
		if(box[idx_tra].type > 0)
			GLog::log(GLOG_INFO, "�û�%d��%d����Ʒ%d�����ʺŲֿ�", _parent->ID.id, box[idx_tra].count, box[idx_tra].type);	
	}
}

void 
gplayer_imp::PlayerTrashItemToInv(int where, size_t idx_tra, size_t idx_inv, size_t count)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}
	//�����ֿ�ȡ����ƷʱҪ�ܰ�ȫ��������
	if(IsPortableTrashBox(where) && !_security_passwd_checked)
	{
		_runner->error_message(S2C::ERR_SECURITY_PASSWD_UNCHECKED);
		return;
	}
	int delta = MoveBetweenItemList(GetTrashInventory(where),_inventory,idx_tra,idx_inv,count);
	if(delta >= 0)
	{
		//�����ֿ���޸Ĳ���Ҫ�򿪲ֿ⣬��Ҫ���Ӹı����
		if(IsPortableTrashBox(where)) IncTrashBoxChangeCounter();
		_runner->trash_item_to_inventory(where, idx_tra,idx_inv,delta);
		
		//���ʺŲֿ��Ƴ�������Ʒ��Ҫ��¼log
		if(where == gplayer_imp::IL_USER_TRASH_BOX)
		{
			GLog::log(GLOG_INFO, "�û�%d��%d����Ʒ%d�Ƴ��ʺŲֿ�", _parent->ID.id, delta, _inventory[idx_inv].type);	
		}
	}
	else
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
	}
}

void 
gplayer_imp::PlayerInvItemToTrash(int where, size_t idx_inv, size_t idx_tra, size_t count)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}
	if(!VerifySpecTrashBox(where, _inventory, idx_inv))
	{
		return ;
	}
	item_list & box = GetTrashInventory(where);
	//�ʺŲֿ���Ʒ�����ɲ�������ֻ����ȡ����Ʒ
	if(where == gplayer_imp::IL_USER_TRASH_BOX && box.GetItemCount() >= box.Size()/2)
	{
		if(box[idx_tra].type == -1)	return;
	}
	int delta = MoveBetweenItemList(_inventory,box,idx_inv,idx_tra,count);
	if(delta >= 0)
	{
		//�����ֿ���޸Ĳ���Ҫ�򿪲ֿ⣬��Ҫ���Ӹı����
		if(IsPortableTrashBox(where)) IncTrashBoxChangeCounter();
		_runner->inventory_item_to_trash(where, idx_inv,idx_tra,delta);
		
		//���ʺŲֿ��Ƴ�������Ʒ��Ҫ��¼log
		if(where == gplayer_imp::IL_USER_TRASH_BOX)
		{
			GLog::log(GLOG_INFO, "�û�%d��%d����Ʒ%d�����ʺŲֿ�", _parent->ID.id, delta, box[idx_tra].type);	
		}
	}
	else
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
	}
}


void 
gplayer_imp::PlayerExchangeTrashMoney(char is_usertrashbox, size_t inv_money, size_t tra_money)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}
	player_trashbox * box;
	if(is_usertrashbox)
		box = &_user_trashbox;
	else
		box = &_trashbox;
	if( (inv_money && tra_money) || (!inv_money && !tra_money) || inv_money > GetMoney()  || tra_money > box->GetMoney())  
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return ;
	}

	if(inv_money)
	{
		//ǮŲ��Ǯ��
		size_t delta = inv_money;
		size_t tm = box->GetMoney();
		size_t rst = tm + delta;
		if(rst < tm || rst > TRASHBOX_MONEY_CAPACITY)
		{
			delta = TRASHBOX_MONEY_CAPACITY - tm;
		}
		box->GetMoney() += delta;
		SpendMoney(delta);
		GLog::log(GLOG_INFO,"�û�%d%s�ֿ�����Ǯ%d",_parent->ID.id, is_usertrashbox?"�ʺ�":"", delta);
		_runner->exchange_trash_money(is_usertrashbox,-(int)delta,delta);
	}
	else
	{
		//��Ǯ�����ó�Ǯ

		//���ȼ��������Ƿ��㹻
		if(!CheckIncMoney(tra_money))
		{
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			return ;
		}

		//ֻ��Ǯ����ʱ��Ż�����Ǯ
		size_t delta = _player_money;
		GainMoney(tra_money);
		delta = _player_money - delta;
		box->GetMoney() -= delta;
		GLog::log(GLOG_INFO,"�û�%d%s�ֿ��Ƴ���Ǯ%d",_parent->ID.id, is_usertrashbox?"�ʺ�":"", delta);
		_runner->exchange_trash_money(is_usertrashbox,delta,-(int)delta);
	}
}


void 
gplayer_imp::PlayerDropInvItem(size_t index,size_t count,bool isProtected)
{
	if(index >= _inventory.Size() || !count)
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}

	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}

	if(world_manager::GetWorldLimit().nothrow)
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return ;
	}

	if(_inventory[index].type == -1)
	{
		_runner->player_drop_item(IL_INVENTORY,index,-1,0,S2C::DROP_TYPE_PLAYER);
		return ;
	}

	if(_inventory[index].proc_type & item::ITEM_PROC_TYPE_NOTHROW) 
	{
		//����Ʒ�޷��������Ƿ񱨸����
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return;
	}

	//������ڿ��е���Ʒ�����Ƿ��㹻
	if(!_plane->CheckPlayerDropCondition())
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return ;
	}

	ThrowInvItem(index,count,isProtected,S2C::DROP_TYPE_PLAYER);
}

void 
gplayer_imp::ThrowInvItem(size_t index, size_t count, bool isProtected,unsigned char drop_type, const XID & spec_owner)
{
	if(world_manager::GetWorldLimit().nothrow_anyway) return;
	item_data data;
	int rst = _inventory.GetItemData(index, data);
	if(rst <=0) 
	{
		ASSERT(false);
		return;
	}
	if(data.count > count)
	{
		data.count = count;
	}

	//���������λ��
	A3DVECTOR pos(_parent->pos);
	pos.x += abase::Rand(-0.5f,+0.5f);
	pos.z += abase::Rand(-0.5f,+0.5f);
	const grid * pGrid = &_plane->GetGrid();
	//��������˱߽磬��ô�Ͱ����Լ���λ������
	if(!pGrid->IsLocal(pos.x,pos.z))
	{
		pos.x = _parent->pos.x;
		pos.z = _parent->pos.z;
	}

	
	//�������ڶ�������Ȼ��item�ͷź������content ���ܾʹ�����
	if(isProtected)
		DropItemFromData(_plane,pos,data,_parent->ID,GetTeamID(),GetTeamSeq(),_parent->ID.id); 
	else if(spec_owner.IsPlayer())
		DropItemFromData(_plane,pos,data,spec_owner,0,0,_parent->ID.id); 
	else
		DropItemFromData(_plane,pos,data,XID(0,0),0,0,_parent->ID.id); 

	GLog::log(LOG_INFO,"�û�%d��������%d��%d",_parent->ID.id,data.count,data.type);

	_inventory.DecAmount(index,data.count);
	_runner->player_drop_item(IL_INVENTORY,index,data.type,data.count,drop_type);
}

void 
gplayer_imp::PlayerDropEquipItem(size_t index,bool isProtected)
{
	if(index >= _equipment.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
		
	if(_lock_equipment)
	{	
		_runner->error_message(S2C::ERR_EQUIPMENT_IS_LOCKED);
		return ;
	}

	if(world_manager::GetWorldLimit().nothrow)
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return ;
	}

	if(_equipment[index].type == -1)
	{
		_runner->player_drop_item(IL_EQUIPMENT,index,-1,0,S2C::DROP_TYPE_PLAYER);
		return ;
	}
	if(_equipment[index].proc_type & item::ITEM_PROC_TYPE_NOTHROW) 
	{
		//����Ʒ�޷��������Ƿ񱨸����
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return;
	}

	//������ڿ��е���Ʒ�����Ƿ��㹻
	if(!_plane->CheckPlayerDropCondition())
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return ;
	}
	
	if(index == item::EQUIP_INDEX_HP_ADDON  || index == item::EQUIP_INDEX_MP_ADDON)
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return;
	}	
	//��С���飬��ֹս��״̬�л���С����  //lgc
	if(index == item::EQUIP_INDEX_ELF)
	{
		if(IsCombatState())	
		{
			_runner->error_message(S2C::ERR_ELF_CANNOT_UNEQUIP_IN_COMBAT_STATE);
			return ;
		}	
	}
	//�ܰ�ȫ���뱣����װ��λ�ò��ܻ���
	if(!_security_passwd_checked)
	{
		if(item::CheckEquipProtectedByIndex(index))
		{
			_runner->error_message(S2C::ERR_SECURITY_PASSWD_UNCHECKED);
			return;
		}
	}

	ThrowEquipItem(index,isProtected,S2C::DROP_TYPE_PLAYER);
}

void 
gplayer_imp::ThrowEquipItem(size_t index, bool isProtected,unsigned char drop_type, int throw_count, const XID & spec_owner)
{
	if(world_manager::GetWorldLimit().nothrow_anyway) return;
	item_data data;
	int rst = _equipment.GetItemData(index, data);
	if(rst <=0) 
	{
		ASSERT(false);
		return;
	}
	if(throw_count > 0 && (int)data.count > throw_count)
	{
		data.count = throw_count;
	}

	//����װ���ı��־
	IncEquipChangeFlag();

	//���������λ��
	A3DVECTOR pos(_parent->pos);
	pos.x += abase::Rand(-0.5f,+0.5f);
	pos.z += abase::Rand(-0.5f,+0.5f);
	const grid * pGrid = &_plane->GetGrid();
	//��������˱߽磬��ô�Ͱ����Լ���λ������
	if(!pGrid->IsLocal(pos.x,pos.z))
	{
		pos.x = _parent->pos.x;
		pos.z = _parent->pos.z;
	}

	//�������ڶ�������Ȼ��item�ͷź������content ���ܾʹ�����
	if(isProtected)
		DropItemFromData(_plane,pos,data,_parent->ID,GetTeamID(),GetTeamSeq(),_parent->ID.id); 
	else if(spec_owner.IsPlayer())
		DropItemFromData(_plane,pos,data,spec_owner,0,0,_parent->ID.id); 
	else
		DropItemFromData(_plane,pos,data,XID(0,0),0,0,_parent->ID.id); 

	GLog::log(LOG_INFO,"�û�%d����װ��%d",_parent->ID.id,data.type);
	_equipment.DecAmount(index, data.count);
	_runner->player_drop_item(IL_EQUIPMENT,index,data.type,data.count,drop_type);

	//����ˢ��װ��
	RefreshEquipment();
	//����������Ϣ
	CalcEquipmentInfo();
	_runner->equipment_info_changed(0,1ULL<<index,0,0); //�˺���ʹ����CalcEquipmentInfo�Ľ��
}


void 
gplayer_imp::PlayerExchangeEquipItem(size_t index1,size_t index2)
{
	size_t size = _equipment.Size();
	if(index1 >= size || index2 >= size || index1 == index2)
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}

	if(_lock_equipment)
	{	
		_runner->error_message(S2C::ERR_EQUIPMENT_IS_LOCKED);
		return ;
	}

	size_t type = ((_equipment[index1].type == -1)?1:0) + ((_equipment[index2].type == -1)?2:0);
	uint64_t mask1 = 1ULL << index1;
	uint64_t mask2 = 1ULL << index2;
	uint64_t mask_add = 0;
	uint64_t mask_del = 0;
	int id[2];
	int count=0;
	switch(type)
	{
		case 0:
			//index1 �� index2��
			if(!(_equipment[index2].CheckEquipPostion(index1)) || !(_equipment[index1].CheckEquipPostion(index2)))
			{
				_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
				return;
			}
			mask_add = mask1 | mask2;
			mask_del = 0;
			count = 2;
			if(index1 > index2)
			{
				id[0] = _equipment[index1].type | _equipment[index1].GetIdModify();
				id[1] = _equipment[index2].type | _equipment[index2].GetIdModify();
			}
			else
			{
				id[1] = _equipment[index1].type | _equipment[index1].GetIdModify();
				id[0] = _equipment[index2].type | _equipment[index2].GetIdModify();
			}
		break;
		case 1:
			//index1 �� index 2��
			if(!(_equipment[index2].CheckEquipPostion(index1)))
			{
				_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
				return;
			}
			mask_add = mask1;
			mask_del = mask2;
			count = 1;
			id[0] = _equipment[index2].type | _equipment[index2].GetIdModify();
		break;
		case 2:
			//index2 �� index 1��
			if(!(_equipment[index1].CheckEquipPostion(index2)))
			{
				_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
				return;
			}
			mask_add = mask2;
			mask_del = mask1;
			count = 1;
			id[0] = _equipment[index1].type | _equipment[index1].GetIdModify();
		break;
		case 3:
			_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
			return;
	}

	IncEquipChangeFlag();

	_equipment.ExchangeItem(index1,index2);
	_runner->exchange_equipment_item(index1,index2);
	
	//ʹ��װ����Ч
	RefreshEquipment();
	//����װ�������ݣ������˿���
	CalcEquipmentInfo();

	_runner->equipment_info_changed(mask_add,mask_del,id, count * sizeof(int)); //�˺���ʹ����CalcEquipmentInfo�Ľ��
}

void 
gplayer_imp::PlayerEquipItem(size_t index_inv, size_t index_equip)
{
	if(index_inv >= _inventory.Size() || index_equip >= _equipment.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}

	if(_lock_equipment)
	{	
		_runner->error_message(S2C::ERR_EQUIPMENT_IS_LOCKED);
		return ;
	}

	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}

	bool type1 = (_inventory[index_inv].type == -1);
	bool type2 = (_equipment[index_equip].type == -1);
	if(type1 && type2)
	{
		_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
		return;
	}

	if(!type2)
	{
		//װ�����ǿ�
		//�������������߻�������ô�����֮
		if(index_equip == item::EQUIP_INDEX_HP_ADDON  || index_equip == item::EQUIP_INDEX_MP_ADDON)
		{
			if(type1)
			{
				//��Щװ������ж��
				_runner->error_message(S2C::ERR_ITEM_CANNOT_UNEQUIP);
				return ;
			}
			if(!_inventory[index_inv].CheckEquipPostion(index_equip))
			{	
				//��Щ��Ʒ����װ��
				_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
				return;
			}

			//���װ���ϵ���������߻�����
			int t1 = _equipment[index_equip].type;
			int proc_type1 = _equipment[index_equip].proc_type;

			_equipment.Remove(index_equip);

			UpdateMallConsumptionDestroying(t1, proc_type1, 1);
			_runner->player_drop_item(IL_EQUIPMENT,index_equip,t1,1,S2C::DROP_TYPE_TAKEOUT);
			type2 = true;
		}
		//��С���飬��ֹս��״̬�л���С����  //lgc
		if(index_equip == item::EQUIP_INDEX_ELF)
		{
			if(IsCombatState())	
			{
				_runner->error_message(S2C::ERR_ELF_CANNOT_UNEQUIP_IN_COMBAT_STATE);
				return ;
			}	
		}

		//�ܰ�ȫ���뱣����װ��λ�ò��ܻ���
		if(!_security_passwd_checked)
		{
			if(item::CheckEquipProtectedByIndex(index_equip))
			{
				_runner->error_message(S2C::ERR_SECURITY_PASSWD_UNCHECKED);
				return;
			}
		}
	}
	
	if(!type1)
	{
		//�������ǿ�
		if(!_inventory[index_inv].CheckEquipPostion(index_equip))
		{	
			//װ��λ�ò���
			_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
			return;
		}
		
		//װ�����в�ͬ��λ���ܴ���ͬ��id�Ķ�̬������Ʒ
		if(item::CheckEquipDynSkillByIndex(index_equip))
		{
			for(size_t i=0; i<_equipment.Size(); i++)
			{
				if( item::CheckEquipDynSkillByIndex(i) && i != index_equip && _equipment[i].type == _inventory[index_inv].type)
				{
					_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
					return;
				}
			}		
		}

		bool is_fly_mode = _layer_ctrl.IsFlying();	//�ж��Ƿ��ڷ���״̬
		//����֮
		if(!EquipItem(index_inv,index_equip))
		{
			_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
		}
		else
		{
			//�����ɹ������һ���Ƿ���Ҫ���а󶨲���
			item & it = _equipment[index_equip];
			ASSERT(it.type != -1);
			bool notify_equip_item = false;
			if(it.proc_type & item::ITEM_PROC_TYPE_BIND2)
			{
				//��������������а�
				it.proc_type |= item::ITEM_PROC_TYPE_NODROP |
					item::ITEM_PROC_TYPE_NOTHROW|
					item::ITEM_PROC_TYPE_NOSELL |
					item::ITEM_PROC_TYPE_NOTRADE|
					item::ITEM_PROC_TYPE_BIND;

				it.proc_type &= ~(item::ITEM_PROC_TYPE_BIND2);
				notify_equip_item = true;
				GLog::log(LOG_INFO,"�û�%dװ������Ʒ%d, GUID(%d,%d)",_parent->ID.id,it.type,it.guid.guid1,it.guid.guid2);

				UpdateMallConsumptionBinding(it.type, it.proc_type, it.count);
			}

			
			int count1 = _inventory[index_inv].count;
			int count2 = _equipment[index_equip].count;
			ASSERT(count1 >= 0 && count2 >=0 && count1+count2 > 0);
			_runner->equip_item(index_inv,index_equip,count1,count2);
			int id1 = _equipment[index_equip].type | _equipment[index_equip].GetIdModify();
			CalcEquipmentInfo();
			_runner->equipment_info_changed(1ULL<<index_equip , 0, &id1,sizeof(id1));//�˺���ʹ����CalcEquipmentInfo�Ľ��

			//���ڷɽ�����������ж�
			if(is_fly_mode && index_equip == item::EQUIP_INDEX_FLYSWORD && !_layer_ctrl.IsFlying())
			{
				//���ηɽ�װ����ʧЧ
				//��ʼ��������ʹ�÷ɽ�
				int type = _equipment[item::EQUIP_INDEX_FLYSWORD].type;
				if(type > 0)
				{
					UseItem(_equipment,item::EQUIP_INDEX_FLYSWORD,IL_EQUIPMENT,type,1);
				}
			}
			
			if(notify_equip_item)
			{
				PlayerGetItemInfo(IL_EQUIPMENT,index_equip);
			}
		}
		IncEquipChangeFlag();
		return ;
	}
	//������Ϊ�գ�����ԭ���ģ���ˢ��װ��
	item  it1;
	_equipment.Remove(index_equip,it1);
	bool bRst = _inventory.Put(index_inv,it1);
	ASSERT(bRst);
	if(bRst)
	{
		it1.Clear();
	}
	else
	{
		//��¼������־
		GLog::log(GLOG_ERR,"װ����Ʒʱ������������");
		it1.Release();
	}
	RefreshEquipment();
	int count1 = _inventory[index_inv].count;
	ASSERT(count1 > 0);
	_runner->equip_item(index_inv,index_equip,count1,0);
	CalcEquipmentInfo();
	_runner->equipment_info_changed(0,1ULL<<index_equip , 0, 0); //�˺���ʹ����CalcEquipmentInfo�Ľ��
	IncEquipChangeFlag();
	
}

void 
gplayer_imp::PlayerMoveEquipItem(size_t index_inv, size_t index_equip)
{
	if(index_inv >= _inventory.Size() || index_equip >= _equipment.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}

	if(_lock_equipment)
	{	
		_runner->error_message(S2C::ERR_EQUIPMENT_IS_LOCKED);
		return ;
	}
	
	item & iteq = _equipment[index_equip];
	if(iteq.type == -1) 
	{
		//װ������Ӧλ�������ݣ����ͬ��װ��
		PlayerEquipItem(index_inv,index_equip);
		return ;
	}

	const item & itin = _inventory[index_inv];
	if(itin.type == -1 || !(itin.CheckEquipPostion(index_equip)) )
	{
		//��Ʒ����Ӧλ�������ݣ����޷�װ��
		//��Ʒ����Ӧ��Ʒ�޷�װ����ָ��λ��Ҳ�޷�װ��
		_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
		return;
	}

	if(itin.type != iteq.type || iteq.count >= iteq.pile_limit)
	{
		//��Ʒ��ƥ��,���������Ѿ����ˣ��޷�װ��
		_runner->error_message(S2C::ERR_ITEM_CANNOT_EQUIP);
		return;
	}
	
	//��̬������Ʒ��������Ӱ�켼�ܵ�Ч��,����Ҫ���¼���
	bool dynskillequip = item::CheckEquipDynSkillByIndex(index_equip);
	if(dynskillequip) iteq.Deactivate(item::BODY, index_equip, this);
	int delta = _equipment.IncAmount(index_equip,itin.count);
	if(delta < 0)
	{
		ASSERT(false);
		return ;
	}
	if(dynskillequip) iteq.Activate(item::BODY, _equipment, index_equip, this);

	IncEquipChangeFlag();
	_inventory.DecAmount(index_inv,delta);
	_runner->move_equipment_item(index_inv,index_equip,delta);
}

int  
gplayer_imp::GetInstanceReenterTimeout()
{
	return world_manager::GetInstance()->GetInstanceReenterTimeout(_plane);
}

int 
gplayer_imp::GetWorldType()
{
	return world_manager::GetInstance()->GetWorldType();
}

void 
gplayer_imp::CalcEquipmentInfo()
{
	_equip_info.data.clear();
	_equipment.GetEquipmentData(_equip_info.mask,_equip_info.data);
	_parent->crc = crc16(_equip_info.data.begin(),_equip_info.data.size()) ^ _equip_info.mask;
}

namespace
{
	struct cardset_counter
	{
		size_t count;
		bool active;
		cardset_counter():count(0),active(false){}
	};
}
void 
gplayer_imp::RefreshEquipment()
{
	const int TEST_EQUIP_LIST[] =
	{
		item::EQUIP_INDEX_WEAPON,
		item::EQUIP_INDEX_HEAD,
		item::EQUIP_INDEX_NECK,
		item::EQUIP_INDEX_SHOULDER,
		item::EQUIP_INDEX_BODY,
		item::EQUIP_INDEX_WAIST,
		item::EQUIP_INDEX_LEG,
		item::EQUIP_INDEX_FOOT,
		item::EQUIP_INDEX_WRIST,
		item::EQUIP_INDEX_FINGER1,
		item::EQUIP_INDEX_FINGER2,
		item::EQUIP_INDEX_PROJECTILE,
		item::EQUIP_INDEX_FLYSWORD,
		item::EQUIP_INDEX_FASHION_BODY,
		item::EQUIP_INDEX_FASHION_LEG,
		item::EQUIP_INDEX_FASHION_FOOT,
		item::EQUIP_INDEX_FASHION_WRIST,
		item::EQUIP_INDEX_RUNE_SLOT,
		item::EQUIP_INDEX_BIBLE,
		item::EQUIP_INDEX_TWEAK,
		item::EQUIP_INDEX_GENERALCARD1,
		item::EQUIP_INDEX_GENERALCARD2,
		item::EQUIP_INDEX_GENERALCARD3,
		item::EQUIP_INDEX_GENERALCARD4,
		item::EQUIP_INDEX_GENERALCARD5,
		item::EQUIP_INDEX_GENERALCARD6,
	};
	size_t count = sizeof(TEST_EQUIP_LIST) / sizeof(int);
	ASSERT(count <= _equipment.Size());
	abase::vector<int , abase::fast_alloc<> > temp_list(_equipment.Size(),0);
	item::LOCATION il = _equipment.GetLocation();
	int empty_slot = count;

	//����ȡ������װ��
	for(size_t i = 0; i < count; i ++ )
	{
		int index = TEST_EQUIP_LIST[i];
		if(_equipment[index].type == -1 || _equipment[index].body == NULL) 
		{
			empty_slot --;
			continue;
		}

		temp_list[index] = 1;
		_equipment[index].Deactivate(il,index,this);
	}
	//�������û������ݣ���װ���ж�֮��
	property_policy::UpdatePlayerLimit(this);
	
	//ѭ������װ���Ƿ���Ч
	int tcount;
	do
	{
		tcount = 0;
		for(size_t i = 0; i < _equipment.Size() ; i++)
		{
			if(!temp_list[i]) continue;
			ASSERT(_equipment[i].type != -1 && _equipment[i].body);
			if(_equipment[i].CanActivate(_equipment,i,this))
			{
				_equipment[i].Activate(il,_equipment,i,this);
				temp_list[i] = 0;
				empty_slot --;
				tcount = 1;

				//���������������������
				property_policy::UpdatePlayerLimit(this);
			}
		}
	}while(tcount && empty_slot);

	//���Կ�����װ
	typedef abase::static_map<int, cardset_counter, item::EQUIP_INDEX_GENERALCARD6-item::EQUIP_INDEX_GENERALCARD1+1> CARDSET_MAP;
	CARDSET_MAP cardset_map;
	abase::vector<const generalcard_set *, abase::fast_alloc<> > temp_list2(_equipment.Size(),NULL);
	for(int i = item::EQUIP_INDEX_GENERALCARD1; i <= item::EQUIP_INDEX_GENERALCARD6; i++)
	{
		item & it = _equipment[i];
		if(it.type <= 0 || !it.IsActive()) continue;
		const generalcard_set * pCardset = generalcard_set_manager::Get(it.type);
		if(pCardset == NULL) continue;

		if(it.IsGeneralCardMatchPos(i))//����ƥ�䵱ǰλ�ã��ż�����װ����
		{
			temp_list2[i] = pCardset;
			cardset_counter & counter = cardset_map[pCardset->id];
			if(++ counter.count >= pCardset->total_count)
			{
				counter.active = true;
				counter.count = pCardset->total_count;	
			}
		}
		else
		{
			//����Ѿ������ȡ��
			if(it.IsSecActive())
			{
				it.Deactivate(il,i, this);
				it.SetSecActive(false, pCardset->enhance);
				it.Activate(il,_equipment,i,this);
			}
		}
	}
	for(int i = item::EQUIP_INDEX_GENERALCARD1; i <= item::EQUIP_INDEX_GENERALCARD6; i++)
	{
		if(!temp_list2[i]) continue;
		item & it = _equipment[i];
		const generalcard_set * pCardset = temp_list2[i];
		cardset_counter & counter = cardset_map[pCardset->id];
		bool active = counter.active && counter.count;	//��༤����װ����������

		if(active != it.IsSecActive())
		{
			it.Deactivate(il,i, this);
			it.SetSecActive(active, pCardset->enhance);
			it.Activate(il,_equipment,i,this);
		}
		if(active) --counter.count;
	}

	//���¼�����������
	property_policy::UpdatePlayer(GetPlayerClass(),this);

	if(_basic.hp > _cur_prop.max_hp) _basic.hp = _cur_prop.max_hp;
	if(_basic.mp > _cur_prop.max_mp) _basic.mp = _cur_prop.max_mp;

	__PRINTF("�����ٶȵ���%f\n",_en_point.flight_speed);
	
	//����װ�������ȼ�
	UpdateEquipRefineLevel();
}

bool
gplayer_imp::EquipItem(size_t index_inv,size_t index_equip)
{	
	ASSERT(index_inv < _inventory.Size());
	ASSERT(_inventory[index_inv].type != -1);
	ASSERT(_inventory[index_inv].CheckEquipPostion(index_equip));

	item & it = _inventory[index_inv];
	item & iteq = _equipment[index_equip];
	//���Ƽ���ʱ������ͳ������Ϊ��ֹ�¿��Ʋ����㼤���������Ȱ�ԭ����ȡ������
	bool need_deactivate = (index_equip >= item::EQUIP_INDEX_GENERALCARD1 && index_equip <= item::EQUIP_INDEX_GENERALCARD6 && iteq.type != -1);
	if(need_deactivate) iteq.Deactivate(item::BODY, index_equip, this);
	if(!it.CanActivate(_equipment,index_equip,this))
	{
		if(need_deactivate) iteq.Activate(item::BODY, _equipment, index_equip, this);
		return false;	
	}

	item  it1;
	item  it2;
	_inventory.Remove(index_inv, it1);
	_equipment.Remove(index_equip, it2);

	_equipment.Put(index_equip,it1);
	_inventory.Put(index_inv,it2);
	it1.Clear();
	it2.Clear();
	
	RefreshEquipment();
	return true;
}

void 
gplayer_imp::PlayerGetInventory(int where)
{
	abase::octets buf;
	unsigned char size;
	switch(where)
	{
		case IL_INVENTORY:
			_inventory.SimpleSave(buf);
			size = _inventory.Size();
			break;
		case IL_EQUIPMENT:
			_equipment.SimpleSave(buf);
			size = _equipment.Size();
			break;
		case IL_TASK_INVENTORY:
			_task_inventory.SimpleSave(buf);
			size = _task_inventory.Size();
			break;
		default:
		ASSERT(false);
		return;
	}
	_runner->self_inventory_data(where, size, buf.begin(), buf.size());
}

void 
gplayer_imp::PlayerDropMoney(size_t amount,bool isProtected)
{
	if(_lock_inventory)
	{	
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return ;
	}

	if(world_manager::GetWorldLimit().nothrow)
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return ;
	}

	if(!_plane->CheckPlayerDropCondition())
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DROP_ITEM);
		return ;
	}
	ThrowMoney(amount,isProtected);
}

void 
gplayer_imp::ThrowMoney(size_t amount, bool isProtected)
{	
	if(world_manager::GetWorldLimit().nothrow_anyway) return;
	if(amount >= _player_money) amount = _player_money;
	if(amount == 0) return ;
	GLog::log(LOG_INFO,"�û�%d������Ǯ%d",_parent->ID.id,amount);
	SpendMoney(amount,false);

	//���������λ��
	A3DVECTOR pos(_parent->pos);
	pos.x += abase::Rand(-0.5f,+0.5f);
	pos.z += abase::Rand(-0.5f,+0.5f);
	const grid * pGrid = &_plane->GetGrid();

	//��������˱߽磬��ô�Ͱ����Լ���λ������
	if(!pGrid->IsLocal(pos.x,pos.z))
	{
		pos.x = _parent->pos.x;
		pos.z = _parent->pos.z;
	}

	if(isProtected)
		DropMoneyItem(_plane,pos,amount,_parent->ID,GetTeamID(),GetTeamSeq(),_parent->ID.id);
	else
		DropMoneyItem(_plane,pos,amount,XID(0,0),0,0,_parent->ID.id);

	_runner->spend_money(amount);
}

void 
gplayer_imp::PlayerGetProperty()
{
	_runner->self_get_property(_basic.status_point,_cur_prop,
								_attack_degree, _defend_degree, 
								_crit_rate+_base_crit_rate, _crit_damage_bonus, 
								((gplayer*)_parent)->invisible_degree, ((gplayer*)_parent)->anti_invisible_degree,
								_penetration, _resilience, GetVigour());
}

void
gplayer_imp::PlayerSetStatusPoint(size_t vit, size_t eng, size_t str, size_t agi)
{
	size_t remain  = _basic.status_point;
	if(str > remain || agi > remain || vit > remain || eng > remain || (str+agi+vit+eng > remain) )
	{
		_runner->set_status_point(0,0,0,0,remain);
		return ;
	}
	player_template::UpdateBasic(GetPlayerClass(),_base_prop,vit,eng,str,agi);
	_cur_prop.vitality += vit;
	_cur_prop.energy   += eng;
	_cur_prop.strength += str;
	_cur_prop.agility  += agi;
	remain -= ( vit + eng + str + agi);
	_basic.status_point = remain;
	RefreshEquipment();
	_runner->set_status_point(vit,eng,str,agi,remain);
}

bool
gplayer_imp::LongJump(const A3DVECTOR &pos,int target_tag, int contrl_id)
{
	if(_player_state != PLAYER_STATE_NORMAL 
			&& _player_state != PLAYER_SIT_DOWN 
			&& _player_state != PLAYER_STATE_MARKET)
	{
		return false;
	}
	if(target_tag == world_manager::GetWorldTag()) 
	{
		LongJump(pos);
		return true;
	}
	instance_key key;
	memset(&key,0,sizeof(key));
	GetInstanceKey(target_tag, key);
	key.target = key.essence;
	key.control_id = contrl_id;

	//��Player���и������� 
	if(world_manager::GetInstance()->PlaneSwitch(this ,pos,target_tag,key,0) < 0)
	{
		return false;
	}
	return true;
}


void 
gplayer_imp::LongJump(const A3DVECTOR &pos)
{
	if(_plane->PosInWorld(pos))
	{
		//Ŀ���������У�ֱ�ӽ�����ת����

		__PRINTF("player goto %f %f\n",pos.x,pos.z);
		_runner->notify_pos(pos);
		A3DVECTOR newpos = pos;
		newpos -= _parent->pos;
		bool bRst = StepMove(newpos);
		//$$$$$$$$�����MoveModeӦ��ʹ�����⴦��
		_runner->stop_move(_parent->pos,0x1000,_parent->dir,0x01);
		ASSERT(bRst);
		//�����Ƿ��ڰ�ȫ��
		TestSanctuary();
		
		if(!world_manager::GetWorldParam().pve_mode)
		{
			//�����Ƿ������ְ�ȫ��
			TestPKProtected();
		}

		_ph_control.Initialize(this);
	}
	else 
	{	
		//Ŀ�겻���Լ��������У�����ȫ������
		//�˲����޷�Ҳ����Խ��tag ��ͬ��λ��
		int dest = _plane->GetGlobalServer(pos);
		if(dest <0)
		{
			_runner->error_message(0);
			return ;
		}
		//���з������л��Ĳ���
		//Ŀǰ�������ﲢû�и����������л�����
		//���Ҫ���Ǻ�����
		_commander->SwitchSvr(dest,_parent->pos,pos,0);
	}
}

void 
gplayer_imp::PlayerGoto(const A3DVECTOR & pos)
{
	LongJump(pos);
}

bool 
gplayer_imp::Resurrect(float exp_reduce)
{
	if(!_parent->IsZombie()) return false;
	if(_player_state != PLAYER_STATE_NORMAL)  return false;
	session_resurrect * pSession= new session_resurrect(this);
	pSession->SetExpReduce(exp_reduce);
	if(AddSession(pSession)) StartSession();
	return true;
}

bool 
gplayer_imp::CanResurrect(int param)
{
	//�ж��Ƿ��ܹ����и������
	//��������״̬���ܹ������
	return (_player_state == PLAYER_STATE_NORMAL || _player_state == PLAYER_SIT_DOWN);
}

int 
gplayer_imp::Resurrect(const A3DVECTOR & pos,bool nomove,float exp_reduce,int world_tag,float hp_factor, float mp_factor, int param)
{
	exp_reduce = exp_reduce * (100 - _resurrect_exp_lost_reduce) * 0.01f;
	if(_kill_by_player) exp_reduce = 0.f;
	//�������� ���ȸ����Լ�
	_parent->b_zombie = false;
	
	//���Լ���hp��mp��Ϊԭʼ��һ��
	_basic.hp = (int)(_cur_prop.max_hp * hp_factor + 0.5f);
	_basic.mp = (int)(_cur_prop.max_mp * mp_factor + 0.5f);

	SetRefreshState();
	_enemy_list.clear();

	((gplayer_controller*)_commander)->OnResurrect();

	//���������е�session
	ClearNextSession();

	
	if(!nomove)
	{
		_runner->resurrect(0);
		LongJump(pos,world_tag);
	}
	else
	{
		//������ʱԭ�ظ���

		//��ʱ�����޵кͲ��ɲ���״̬(�����µ�session) 
		AddSession(new session_resurrect_protect(this));
		StartSession();


	}
	int exp = player_template::GetLvlupExp(GetPlayerClass(),_basic.level);
	int rexp = (int)(exp * exp_reduce + 0.5f);
	if(rexp > 0)
	{
		int new_exp = (_basic.exp - rexp);
	//	if(new_exp < -exp) new_exp = -exp;
		if(new_exp < 0) new_exp = 0;	//����ֵ�����ɸ���...
		if(_basic.level >= player_template::GetMaxLevel()) new_exp = 0;
//		_runner->receive_exp(new_exp - _basic.exp,0);
		rexp = new_exp - _basic.exp;
		_basic.exp = new_exp;
	}

	//��¼������־
	GLog::log(GLOG_INFO,"�û�%d��������(%d) ��ʧ����%d(%2.2f)", _parent->ID.id, nomove?1:0,rexp,exp_reduce);
	return 0;
}

int 
gplayer_imp::DispatchCommand(int cmd_type, const void * buf,size_t size)
{
	ASSERT(_commander);
	switch(_player_state)
	{
		case PLAYER_STATE_NORMAL:
			if((gplayer*)_parent->IsZombie())
				return ((gplayer_controller*)_commander)->ZombieCommandHandler(cmd_type,buf,size);
			else if(_idle_mode_flag + _seal_mode_flag)
				return ((gplayer_controller*)_commander)->SealedCommandHandler(cmd_type,buf,size);
			else
				return _commander->CommandHandler(cmd_type,buf,size);
		break;

		case PLAYER_STATE_COSMETIC:
			//��������ģʽ�ܹ�����ʲô��Ϣ
			return ((gplayer_controller*)_commander)->CosmeticCommandHandler(cmd_type,buf,size);
		break;

		case PLAYER_WAITING_FACTION_TRADE:
		case PLAYER_WAITING_TRADE:
		case PLAYER_WAIT_FACTION_TRADE_READ:
		case PLAYER_TRADE:
		case PLAYER_WAIT_TRADE_COMPLETE:
		case PLAYER_WAIT_TRADE_READ:
		case PLAYER_WAIT_LOGOUT:
		case PLAYER_DISCONNECT:
		break;
		case PLAYER_SIT_DOWN:
			return ((gplayer_controller*)_commander)->StayInCommandHandler(cmd_type,buf,size);
		break;

		case PLAYER_STATE_BIND:
			if((gplayer*)_parent->IsZombie())
				return ((gplayer_controller*)_commander)->ZombieCommandHandler(cmd_type,buf,size);
			else
				return ((gplayer_controller*)_commander)->BoundCommandHandler(cmd_type,buf,size);
		break;

		case PLAYER_STATE_MARKET:
			return ((gplayer_controller*)_commander)->MarketCommandHandler(cmd_type,buf,size);
		break;
		
		case PLAYER_STATE_TRAVEL:
			return ((gplayer_controller*)_commander)->TravelCommandHandler(cmd_type,buf,size);
		break;
		

		case PLAYER_WAIT_SWITCH:
		//������ʱ����������µ�����,�������ĳ���
		break;
		default:
		ASSERT(false);
	}
	return 0;
}

void 
gplayer_imp::OnPickupMoney(size_t money,int drop_id)
{	
	if(drop_id)
	{
		GLog::log(GLOG_INFO,"�û�%d�����Ǯ%d[�û�%d����]",_parent->ID.id,money,drop_id);
	}
	else
	{
		GLog::log(GLOG_INFO,"�û�%d�����Ǯ%d",_parent->ID.id,money);
	}
	size_t nmoney = _player_money;
	GainMoneyWithDrop(money,false);	//���治�ټ�¼��־
	if(_player_money > nmoney)
	{
		_runner->pickup_money(_player_money - nmoney);
	}
	if((int)_player_money < 0) _player_money = 0x7FFFFFFF;
	__PRINTF("������Ǯ%u\n",_player_money);
}

void
gplayer_imp::PickupMoneyInTrade(size_t money)
{
	((gplayer_dispatcher*)_runner)->pickup_money_in_trade(money);
	_trade_obj->PickupMoney(money);
}

void 
gplayer_imp::PickupItemInTrade(const A3DVECTOR & pos,const void * data, size_t size,bool isTeam, int drop_id)
{
	if(size < sizeof(item_data)) return ;
	item_data *pData = (item_data*)data;
	if(size != sizeof(item_data) + pData->content_length)
	{
		ASSERT(false && " invalid item data size");
		return;
	}
	pData->item_content = (char*)data + sizeof(item_data);
	int ocount = pData->count;

	if(drop_id)
	{
		GLog::log(GLOG_INFO,"�û�%d����ʱ����%d��%d[�û�%d����]",_parent->ID.id,ocount, pData->type,drop_id);
	}
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����ʱ����%d��%d",_parent->ID.id,ocount, pData->type);
		FirstAcquireItem(pData);
	}


	if(isTeam && _team.IsInTeam() && _team.IsRandomPickup())
	{
		//�������ȡ֪ͨ���� �쵽����Ʒ
		_team.NotifyTeamPickup(pos,pData->type,pData->count);
	}

	int rst = _trade_obj->PickupItem(pData);
	if(rst >=0)
	{
		//���ͼ�����Ʒ����Ϣ
		//Ҫע����ܻ�ʣ����Ʒ
		__PRINTF("��������Ʒ%d %d to %d\n",pData->type,pData->classid,rst);
		((gplayer_dispatcher*)_runner)->pickup_item_in_trade(pData->type,ocount - pData->count);
	}

	if(pData->count)
	{
		//û����ȫ������,Ӧ�ý���Ʒ�����׳�
		//δ���ܹ������׳�(���ڷ�����Ʒ��ԭ��)
		DropItemFromData(_plane,_parent->pos,*pData,_parent->ID,0,0,drop_id);
		__PRINTF("����ʧ�ܣ�δ��ȫ����%d %d %d/%d\n",pData->type,pData->classid,pData->count,ocount);
	}
}

void 
gplayer_imp::PurchaseItem(abase::pair<const item_data *, int>* pItem,size_t size,size_t money, int consume_contrib, int force_contrib)
{
	ASSERT(_player_money >= money && size && GetFactionConsumeContrib() >= consume_contrib && _player_force.GetContribution() >= force_contrib && "����֮ǰ�Ѿ��������"  );
	int rst = 0;
	packet_wrapper h1(128);
	using namespace S2C;
	CMD::Make<CMD::player_purchase_item>::FirstStep(h1,money,(size_t)0,size);

	char logtxt[24*14+512];	//������๺��12����Ʒ
	int index = 0;

	for(size_t i = 0; i < size; i ++,pItem++)
	{
		int count = pItem->second;
		rst =_inventory.Push(*pItem->first,count,0);
		ASSERT(rst >= 0 && count == 0);
        _inventory[rst].InitFromShop();

		CMD::Make<CMD::player_purchase_item>::SecondStep(h1,pItem->first, pItem->second,rst);
		index += snprintf(logtxt + index,sizeof(logtxt) - index,"%d��%d,",pItem->second,pItem->first->type);
	}
	ASSERT(index);
	index --;
	logtxt[index] = 0;
	GLog::log(GLOG_INFO,"�û�%d��NPC������%s",_parent->ID.id, logtxt);
	SpendMoney(money);
	if(consume_contrib > 0) DecFactionContrib(consume_contrib,0);
	if(force_contrib > 0) DecForceContribution(force_contrib);
	send_ls_msg(GetParent(), h1);
}

void 
gplayer_imp::OnPickupItem(const A3DVECTOR &pos,const void * data, size_t size,bool isTeam,int drop_id)
{
	if(size < sizeof(item_data)) return ;
	item_data *pData = (item_data*)data;
	if(size != sizeof(item_data) + pData->content_length)
	{
		ASSERT(false && " invalid item data size");
		return;
	}
	pData->item_content = (char*)data + sizeof(item_data);
	int ocount = pData->count;

	if(drop_id)
	{
		GLog::log(GLOG_INFO,"�û�%d����%d��%d[�û�%d����]",_parent->ID.id,ocount, pData->type,drop_id);
	}
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����%d��%d",_parent->ID.id,ocount, pData->type);
		FirstAcquireItem(pData);
	}

	if(isTeam && _team.IsInTeam())// && _team.IsRandomPickup())
	{
		//�������ȡ֪ͨ���� �쵽����Ʒ
		_team.NotifyTeamPickup(pos,pData->type,pData->count);
	}

	int rst = _inventory.Push(*pData);
	if(rst >=0)
	{
		//���ͼ�����Ʒ����Ϣ
		//Ҫע����ܻ�ʣ����Ʒ
		__PRINTF("��������Ʒ%d %d to %d\n",pData->type,pData->classid,rst);
		_runner->pickup_item(pData->type,pData->expire_date, ocount - pData->count,_inventory[rst].count, 0,rst);
		if(pData->proc_type & item::ITEM_PROC_TYPE_AUTO_USE)
		{
			//�Զ�ʹ��
			UseItem(_inventory, rst, IL_INVENTORY, pData->type, 1);
		}
	}

	if(pData->count)
	{
		//û����ȫ������,Ӧ�ý���Ʒ�����׳�
		if(isTeam)
			DropItemFromData(_plane,_parent->pos,*pData,_parent->ID,0,0,drop_id);
		else
			DropItemFromData(_plane,pos,*pData,_parent->ID,0,0,drop_id);
		__PRINTF("����ʧ�ܣ�δ��ȫ����%d %d %d/%d\n",pData->type,pData->classid,pData->count,ocount);
	}
}

bool
gplayer_imp::ObtainItem(int where, item_data * pData, bool isTask)
{
	ASSERT(where != IL_EQUIPMENT);
	int ocount = pData->count;
	item_list & inv = GetInventory(where);
	int rst = inv.Push(*pData);
	if(rst >=0)
	{
		if(isTask)
			_runner->task_deliver_item(pData->type,pData->expire_date,ocount - pData->count,inv[rst].count, where,rst);
		else
			_runner->obtain_item(pData->type,pData->expire_date,ocount - pData->count,inv[rst].count, where,rst);

		if(where == IL_INVENTORY && pData->proc_type & item::ITEM_PROC_TYPE_AUTO_USE)
		{
			UseItem(_inventory, rst, IL_INVENTORY, pData->type, 1);
		}
	}
	FirstAcquireItem(pData);

	if(pData->count && !isTask)
	{
		//Ӧ�ò���ʣ����Ʒ�Ŷ�
		//����Ϊ�˱��գ������ж�һ��
		DropItemData(_plane,_parent->pos,pData,_parent->ID,0,0,0);
		return false;
	}
	return true;
}


bool
gplayer_imp::StepMove(const A3DVECTOR &offset)
{
	_direction = offset;
	_is_moved = true;
	bool bRst = false;
	if((bRst = gobject_imp::StepMove(offset)))
	{
		TestUnderWater();
		if(_player_state == PLAYER_STATE_BIND)
		{
			if(_bind_player.IsPlayerLinked())
			{
				//ֻ�����汻����˲Żᷢ��Ϣ 
				SendTo<0>(GM_MSG_PLAYER_BIND_FOLLOW,_bind_player.GetLinkedPlayer(),0);
			}
		}
		UpdatePlayerLayer();
		//����ƶ�ʱ��������ʱ��
		SetActiveStateDelay(MAX_ACTIVE_STATE_DELAY);
	}
	return bRst;
}

class PlayerForceOfflineTask :public ONET::Thread::Runnable
{
	gplayer * _player;
	int _uid;
	int _cs_sid;
	int _cs_index;
public:
	PlayerForceOfflineTask(gplayer *pPlayer):_player(pPlayer)
						 ,_uid(pPlayer->ID.id),_cs_sid(pPlayer->cs_sid)
						 ,_cs_index(pPlayer->cs_index)
	{
	}
	
	virtual void Run()
	{
		spin_autolock keeper(_player->spinlock);
		if(!_player->IsActived() || _player->ID.id != _uid  ||
				_player->cs_index != _cs_index || _player->cs_sid != _cs_sid || !_player->imp)
		{
			//����û����������Ѿ���ʧ�ˣ�����ֱ�ӷ��أ������д���
			GLog::log(GLOG_WARNING,"�û�%d�쳣����ʱ���û��Ѿ���ʧ",_uid);
			return;
		}
		GLog::log(GLOG_WARNING,"�û�%d��Ϊ���ݴ�������쳣���߲���",_uid);
		_player->imp->PlayerForceOffline();
		delete this;
	}
};

void 
gplayer_imp::OnHeartbeat(size_t tick)
{
//	__PRINTF("player %d heart beat\n", _parent->ID.id);
	_filters.EF_Heartbeat(tick);
	if(_combat_timer)
	{
		_combat_timer --;
		if(_combat_timer <=0)
		{
			ActiveCombatState(false);
			_combat_timer = 0;
		}
	}
	if(_pvp_enable_flag)
	{
		//����PK����ʱ
		if((--_pvp_cooldown) < 0) _pvp_cooldown = 0;
		//if((_pvp_cooldown-=100) < 0) _pvp_cooldown = 0;
	}

	_plane->InspireNPC<0>(_parent->pPiece,0);

	bool bSaveMode = _player_state == PLAYER_STATE_NORMAL || _player_state == PLAYER_SIT_DOWN || _player_state == PLAYER_STATE_MARKET || _player_state == PLAYER_STATE_BIND; 	//���ʾ��ǰ�Ƿ���Դ���
	bool is_zombie = _parent->IsZombie();
	int cur_time = g_timer.get_systime();

	//auto gen hp/mp
	if(!is_zombie)
	{
		bool no_amulet = GetPlayerLimit(PLAYER_LIMIT_NOAMULET);
		//���л�Ѫҩ�ͻ�ħҩ�ļ��
		if(_auto_hp_value  > 0 && !no_amulet)
		{
			if(_auto_hp_percent * _cur_prop.max_hp > _basic.hp)
			{
				int offset = _cur_prop.max_hp - _basic.hp;
				_auto_hp_value = AutoGenStat(COOLDOWN_INDEX_AUTO_HP,
						item::EQUIP_INDEX_HP_ADDON,offset, bSaveMode);
			}
		}

		if(_auto_mp_value  > 0 && !no_amulet)
		{
			if(_auto_mp_percent * _cur_prop.max_mp > _basic.mp)
			{
				int offset = _cur_prop.max_mp - _basic.mp;
				_auto_mp_value = AutoGenStat(COOLDOWN_INDEX_AUTO_MP,
						item::EQUIP_INDEX_MP_ADDON,offset, bSaveMode);
			}
		}

		if(IsCombatState())
		{
			GenHPandMP(_cur_prop.hp_gen,_cur_prop.mp_gen);
		}
		else
		{
			GenHPandMP(_cur_prop.hp_gen*4,_cur_prop.mp_gen*4);
		}
		_breath.OnHeartbeat(this);

	}
	
	if(!is_zombie && (cur_time & 0x03) == 0)
	{
		//ÿ4�������һ�γ��
		ENEMY_LIST::iterator it = _enemy_list.end(); 
		for(;it > _enemy_list.begin(); )
		{
			--it;
			if(it->second <=0) 
			{
				__PRINTF("ɾ���˳�ʱ�ĳ�޼�¼ ����%x\n",it->first);
				it = _enemy_list.erase(it);
			}
			else
			{
				__PRINTF("����%x�ĳ�޼�¼ ��%d������\n",it->first,it->second);
				it->second = 0;
			}
		}
	}

	//���������ԭ�����ڻ�Ѫ�ͼ�Ѫ֮��������ݸ���֮ǰ��������������ȷ�ķ�������
	_team.OnHeartbeat();
	_invade_ctrl.OnHeartbeat();

	_touch_order.OnHeartbeat(this);
	_player_giftcard.OnHeartbeat(this);
	_player_title.OnHeartbeat(cur_time);	
	_player_fatering.OnHeartbeat(cur_time);
    _player_sanctuary_check.OnHeartbeat(this);
	_player_clock.OnHeartbeat(this,cur_time,InCentralServer());

	if(_expire_item_date && bSaveMode)
	{       
		if(cur_time >= _expire_item_date)
		{       
			//Ӧ�ý���ɾ��������
			RemoveAllExpireItems();
		}       
	}
	
	//lgc
	if(!is_zombie)
	{
		if(_cur_elf_info.id != (unsigned int)-1)
		{
			//С����ظ�Ԫ��  
			if(_cur_elf_info.vigor < _cur_elf_info.max_vigor)
			{
				_cur_elf_info.vigor += _cur_elf_info.vigor_gen;
				if(_cur_elf_info.vigor > _cur_elf_info.max_vigor)
					_cur_elf_info.vigor = _cur_elf_info.max_vigor;
				_cur_elf_info.refresh_vigor = true;
			}
			//С����ľ���������������
			if(_cur_elf_info.refine_effect_active)
			{
				item & it = _equipment[item::EQUIP_INDEX_ELF];
				int second_cost = elf_refine_effect_table[_cur_elf_info.refine_level].std_cost * (_basic.level  + 105)/210;
				int cur_stamina = it.GetStamina();
				if(cur_stamina <= second_cost)
				{
					ElfRefineDeactivate(_cur_elf_info.refine_level);
					_runner->elf_refine_activate(0);

					it.DecStamina(cur_stamina);
					//֪ͨ�ͻ��˼�������
					_runner->query_elf_stamina(0);
				}
				else
				{
					it.DecStamina(second_cost);
					//֪ͨ�ͻ��˼�������
					_runner->query_elf_stamina(cur_stamina - second_cost);
				}
			}
		}
	}
	// ����Ƿ���С�����ת��״̬�ѽ���
	if(_min_elf_status_value > 0 && bSaveMode)
	{       
		if(cur_time >= _min_elf_status_value)
			UpdateAllElfSecureStatus();
	}
	//����Ƿ��и��������ѹ���
	if(_min_addon_expire_date > 0 && bSaveMode)
	{
		if(cur_time >= _min_addon_expire_date)
			RemoveAllExpireAddon();
	}
	
	//�ж��Ƿ���� ֻ����ͨģʽ��������ж�
	if(--_write_timer <= 0)
	{
		if(bSaveMode)
		{
			AutoSaveData();
		}
		_write_timer = abase::Rand(500, 513);
	}

	//�ж��Ƿ������������Ϣ
	if(--_link_notify_timer <= 0)
	{

		//����Ҫ�ж��Լ��Ƿ��ܹ���ѯ���Լ���һ���취���ڴ��̵�ʱ���飩
		//��������޷���ѯ���Լ��Ļ��������������⣬������������ɾ������
		//��ΪHeartbeat��ʱ����Ҫ����һЩ����
		gplayer *pPlayer = GetParent();
		int index1;
		gplayer * pPlayer2 = world_manager::GetInstance()->FindPlayer(pPlayer->ID.id,index1);
		if(pPlayer2 != pPlayer || pPlayer2 == NULL)
		{
			//�������뽫�Լ��ͷ��Ҳ�����
			//����һ�����ܵ�����
			GLog::log(GLOG_WARNING,"%d�û�%d(%d,%d)���ݷ������󣬿�ʼ�쳣����(%d)",world_manager::GetWorldTag(),pPlayer->ID.id,pPlayer->cs_index, pPlayer->cs_sid,pPlayer2?1:0);
			ONET::Thread::Pool::AddTask(new PlayerForceOfflineTask(pPlayer));
		}
		else if(_player_state != PLAYER_DISCONNECT)
		{
			_link_notify_timer = LINK_NOTIFY_TIMER;
			GMSV::SendPlayerHeartbeat(pPlayer->cs_index,pPlayer->ID.id, pPlayer->cs_sid);
			__PRINTF("����������link������ %d\n",pPlayer->ID.id);
			//�����¼��־�ƺ������ ������һ�¼�¼��־�İ취

			//���������������־����
			if(abase::Rand(0,5) == 0)
			{
				GLog::log(GLOG_INFO,"%d�����û�%d(%d,%d)������Ϣ",world_manager::GetWorldTag(),pPlayer->ID.id,pPlayer->cs_index,pPlayer->cs_sid);
			}

			bool pos_log = false;
			if(_player_state == PLAYER_STATE_NORMAL)
			{
				if(abase::Rand(0,4) == 0)
				{
					pos_log = true;
				}
			}
			else
			{
				if(abase::Rand(0,8) == 0)
				{
					pos_log = true;
				}
			}
			if(pos_log)
			{
				GLog::log(GLOG_INFO,"�û�%d����λ��(%f,%f,%f)[%d],״̬%d",_parent->ID.id,_parent->pos.x,_parent->pos.y,_parent->pos.z,world_manager::GetWorldTag(), _layer_ctrl.GetLayer());
			}
		}


	}

	//�ж��Ƿ�˫��ʱ��
	if(_double_exp_mode)
	{
		if(cur_time > _double_exp_timeout)
		{
			LeaveDoubleExpMode();
		}
	}

	PVPCombatHeartbeat();
	_duel.Heartbeat(this);
	_bind_player.Heartbeat(this);
	_petman.Heartbeat(this);
	_plantpetman.Heartbeat(this);
	_meridianman.Heartbeat(this,cur_time);
/*
	//��������ϵͳ
	if(world_manager::AntiWallow())
	{
		int new_wallow = _wallow_obj.Tick(g_timer.get_systime());
		if(new_wallow != _wallow_level)
		{
			//����״̬�����仯
			_wallow_level = new_wallow;

			//֪ͨ�ͻ���
			time_t l_time;
			time_t h_time;
			int ptime;
			_wallow_obj.GetTimeLeft(&l_time, &h_time, &ptime);
			_runner->player_wallow_info(_wallow_level, ptime, l_time, h_time);
		}
	}
	*/

	if(_cheat_mode > 0)
	{
		_cheat_mode --;
		if(_cheat_mode <= 0) 
		{
			_cheat_mode = -1;
			PunishCheat();
		}
	}

	if(world_manager::GetWorldLimit().height_limit)
	{
		if(_parent->pos.y >= world_manager::GetHeightLimit())
		{               
			_parent->pos.y = 0;
		}                       
	}

	if(cur_time - _profit_timestamp >= TOTAL_SEC_PER_DAY)
	{
		//��������ʱ��
		time_t now = g_timer.get_systime();
		struct tm *tm_now = localtime(&now);
		ASSERT(tm_now);
		_profit_time = world_manager::GetWorldConfig().profit_time;
		_profit_timestamp = now - tm_now->tm_hour*3600 - tm_now->tm_min*60 - tm_now->tm_sec;
		CalcProfitLevel();
		_runner->update_profit_time(S2C::CMD::player_profit_time::PROFIT_LEVEL_CHANGE,_profit_time, _profit_level);
	}

	if(world_manager::ProfitTimeLimit())
	{
		if(IsCombatState() && IsAttackMonster())
		{
			//���������ͼ1
			//ս�����ʱ��������ʱ��
			UpdateProfitTime();
		}
	}
	else if(world_manager::ProfitTimeLimit2())
	{
		if(_active_state_delay > 0)
		{
			//���������ͼ2
			//ս����������ֹͣ�ƶ�����ӳ�����
			UpdateProfitTime();
			if(--_active_state_delay <= 0)
			{
				_runner->notify_profit_state(0);
			}
		}
	}

	_multi_exp_ctrl.Update(this,cur_time);
	_online_award.Update(this,cur_time);

	if(_country_expire_time && cur_time >= _country_expire_time)
	{
		GMSV::CountryBattleLeave(_parent->ID.id, GetCountryId(), Get16Por9JWeapon(), GetSoulPower());
		SetCountryId(0,0);
	}
	if(_king_expire_time && cur_time >= _king_expire_time)
	{
		SetKing(false, 0);	
	}

	if(_need_refresh_equipment)
	{
		RefreshEquipment();
		PlayerGetProperty();
		_need_refresh_equipment = false;
	}
}

void gplayer_imp::TryClearTBChangeCounter()
{
	_tb_change_counter = _trash_box_open_flag?1:0;
}

void gplayer_imp::TryClearUserTBChangeCounter()
{
	_user_tb_change_counter = _user_trash_box_open_flag?1:0;
}

void 
gplayer_imp::AutoSaveData()
{
	//write
	class AutoWrite: public GDB::Result, public abase::ASmallObject
	{	
		world * _plane;
		int _userid;
		unsigned int _counter;
		unsigned int _counter2; 
		unsigned int _counter3;
		int _mall_order_id;
	public:
		AutoWrite(gplayer_imp * imp)
		{
			_plane = imp->_plane;
			_userid = imp->_parent->ID.id;
			_counter = imp->_tb_change_counter;
			_counter2 = imp->_eq_change_counter;
			_counter3 = imp->_user_tb_change_counter;
			_mall_order_id = imp->_mall_order_id;
		}

		//ƽʱ���̲�������
		virtual void OnTimeOut() { OnFailed();}
		//�����ܵ�������� 
		virtual void OnFailed() {
			GLog::log(GLOG_WARNING,"��%d�Զ����̱����û�%d����ʧ��",world_manager::GetWorldTag(),_userid);
			MSG msg;
			BuildMessage(msg,GM_MSG_DBSAVE_ERROR,XID(GM_TYPE_PLAYER,_userid),XID(GM_TYPE_PLAYER,_userid),A3DVECTOR(0,0,0));
			//���_plane��һ������ʹ�� ������manager�Ľӿ������
			world_manager::GetInstance()->SendRemotePlayerMsg(_userid,msg);
			delete this;
		}

		virtual void OnPutRole(int retcode){
			//��ͼѰ��һ����ң�����Ʒ���д���־��Ϊfalse
			ASSERT(retcode == 0);

			int index = _plane->FindPlayer(_userid);
			if(index >= 0)
			{
				gplayer * pPlayer = _plane->GetPlayerByIndex(index);
				spin_autolock keeper(pPlayer->spinlock);
				if(pPlayer->IsActived() && pPlayer->imp && pPlayer->ID.id == _userid
						&& pPlayer->login_state >= gplayer::LOGIN_OK)
				{
					gplayer_imp * pImp = (gplayer_imp*)pPlayer->imp;
					//ֻ��û���ٴδ򿪹���û�����ڴ����ӣ��Ż�������ӵĸ��ļ��� 
					if(_counter && pImp->_tb_change_counter == _counter)
					{
						pImp->TryClearTBChangeCounter();
					}

					if(_counter2 == pImp->_eq_change_counter )
					{
						pImp->_eq_change_counter = 0;
					}
					//ֻ��û���ٴδ򿪹���û�����ڴ����ӣ��Ż�������ӵĸ��ļ��� 
					if(_counter3 && pImp->_user_tb_change_counter == _counter3)
					{
						pImp->TryClearUserTBChangeCounter();
					}

					pImp->MallSaveDone(_mall_order_id);

					//�����̴�����0
					pImp->_db_save_error = 0;
				}
				
			}
			GLog::log(GLOG_INFO,"�û�%d�����Զ��������",_userid);
			delete this;
		}
	};
	_write_counter ++;
	user_save_data((gplayer*)_parent,new AutoWrite(this),1);
}

void
gplayer_imp::OnAttacked(world *pPlane,const MSG & msg,attack_msg *attack,damage_entry &dmg,bool is_hit)
{
	//����Ӧ�����ܶ��жϣ���ʱ�ȿ���ս��״̬
	ActiveCombatState(true);
	if(_combat_timer < NORMAL_COMBAT_TIME)
	{
		_combat_timer = NORMAL_COMBAT_TIME;
	}

	_petman.PlayerBeAttacked(this, msg.source);
	_plantpetman.PlayerBeAttacked(this, msg.source);
}

bool 
gplayer_imp::UseProjectile(int count)
{
	if(_equipment[item::EQUIP_INDEX_PROJECTILE].type == -1 ||
			_equipment[item::EQUIP_INDEX_PROJECTILE].count < (size_t)count)
	{
		return false;
	}

	__PRINTF("ʹ���˼�֧%d\n",count);

	if(!_equipment.DecAmount(item::EQUIP_INDEX_PROJECTILE,count))
	{	
		//�������ˢ�£���Ϊ���ܻ���һ�����Ե�Ӱ��
		RefreshEquipment();
	}
	return true;
}

void 
gplayer_imp::OnHurt(const XID & attacker,const attacker_info_t&info,int damage,bool invader)
{
	//����ս��״̬����Ϣ
	ActiveCombatState(true);
	if(_combat_timer < NORMAL_COMBAT_TIME)
	{
		_combat_timer = NORMAL_COMBAT_TIME;
	}

	_runner->be_hurt(attacker, info, damage,invader);
}

void 
gplayer_imp::OnDamage(const XID & attacker,int skill_id, const attacker_info_t&info,int damage,int at_state,char speed,bool orange,unsigned char section)
{
	//����ս��״̬����Ϣ
	ActiveCombatState(true);
	if(_combat_timer < NORMAL_COMBAT_TIME)
	{
		_combat_timer = NORMAL_COMBAT_TIME;
	}

	int index = SelectRandomArmor();
	if(index >= 0 && _equipment.DecDurability(index, DURABILITY_DEC_PER_HIT))
	{
		_runner->equipment_damaged(index,0);
		//��֪��Ʒ������
		RefreshEquipment();
	}
	_runner->be_damaged(attacker,skill_id, info, damage,index,at_state,speed,orange,section);
}

int
gplayer_imp::SelectRandomArmor()
{
	int index = abase::Rand(item::EQUIP_ARMOR_START,item::EQUIP_ARMOR_END-1);
	if (_equipment[index].type == -1) return -1;
	return index;
/*
	unsigned short mask = _equip_info.mask & item::EQUIP_ARMOR_ALL_MASK;
	if(!mask) return -1;
	int index = abase::Rand(item::EQUIP_ARMOR_START,item::EQUIP_ARMOR_END-1);
	unsigned short pos = 1 << index;
	if(mask & pos) return index; //�ҵ�
	if(mask & (pos -1))
	{
		//�ڵ�λѡ��
		do
		{
			pos >>= 1;
			index --;
			if(mask & pos) return index;
			ASSERT(index > item::EQUIP_ARMOR_START);
		}while(index > item::EQUIP_ARMOR_START);
	}
	else
	{
		//�ڸߴ�ѡ��
		do
		{
			pos <<= 1;
			index ++;
			if(mask & pos) return index;
			ASSERT(index < item::EQUIP_ARMOR_END -1);
		}while(index < item::EQUIP_ARMOR_END -1);
	}
	return -1;
	*/
}

	

void 
gplayer_imp::AdjustDamage(const MSG & msg, attack_msg * attack,damage_entry & dmg, float & damage_adjust)
{
	if(IS_HUMANSIDE(attack->ainfo.attacker))
	{
		int pp = (((attack->attacker_layer) & 0x03) << 2) | _layer_ctrl.GetLayer();
		ASSERT((_layer_ctrl.GetLayer() & ~0x03) == 0);
		switch(pp)
		{
			case ((LAYER_GROUND << 2) | LAYER_GROUND):
			case ((LAYER_AIR << 2) | LAYER_AIR):
			case ((LAYER_WATER << 2) | LAYER_WATER):
			case ((LAYER_GROUND << 2) | LAYER_AIR):
			case ((LAYER_GROUND << 2) | LAYER_WATER):
			case ((LAYER_WATER << 2) | LAYER_GROUND):
			case ((LAYER_WATER << 2) | LAYER_AIR):
				damage_adjust *= PVP_DAMAGE_REDUCE;
				break;

			case ((LAYER_AIR << 2) | LAYER_GROUND):
			case ((LAYER_AIR << 2) | LAYER_WATER):
				damage_adjust *= PVP_DAMAGE_REDUCE * 0.5f;
				break;

			case ((LAYER_INVALID<< 2) | LAYER_GROUND):
			case ((LAYER_INVALID<< 2) | LAYER_AIR):
			case ((LAYER_INVALID<< 2) | LAYER_WATER):
			case ((LAYER_INVALID<< 2) | LAYER_INVALID):
			case ((LAYER_GROUND << 2) | LAYER_INVALID):
			case ((LAYER_AIR << 2) | LAYER_INVALID):
			case ((LAYER_WATER << 2) | LAYER_INVALID):
				ASSERT(false);
				break;
			default:
				ASSERT(false);
		}
	}
	else
	{
		//���ֹ���
		damage_adjust *= (1.f - player_template::GetResilienceImpair(_resilience,attack->ainfo.level));
	}
	
	if(attack->skill_id && attack->skill_enhance2)
		damage_adjust *= (0.01f * (100 + attack->skill_enhance2));
}

void 
gplayer_imp::SendServiceRequest(int service_type,const void * buf, size_t length)
{
	service_executor * executor;
	if(CanUseService() && (executor = service_manager::GetExecutor(service_type)) )
	{
		if(!executor->ServeRequest(this,_provider.id, buf,length))
		{
			_runner->error_message(S2C::ERR_SERVICE_ERR_REQUEST);
		}
	}
	else
	{
		_runner->error_message(S2C::ERR_SERVICE_UNAVILABLE);
	}
}

void 
gplayer_imp::QueryServiceContent(int service_type)
{
	if(CanUseService())
	{	
		struct 
		{
			int cs_index;
			int sid;
		} data;
		gplayer * pPlayer = (gplayer * )_parent;
		data.cs_index = pPlayer->cs_index;
		data.sid =  pPlayer->cs_sid;
		SendTo<0>(GM_MSG_SERVICE_QUIERY_CONTENT,_provider.id,service_type,&data,sizeof(data));
	}
	else
	{
		_runner->error_message(S2C::ERR_SERVICE_UNAVILABLE);
	}
}

void 
gplayer_imp::RepairAllEquipment()
{
	int item_count = 0;
	size_t cost = 0;
	int count ;
//	cost += _inventory.GetRepairCost(count);
//	item_count += count;
	cost += _equipment.GetRepairCost(count);
	item_count += count;
	if(count > 0)
	{
		if(cost == 0) cost = 1;
		if(cost < _player_money)
		{
//			_inventory.RepairAll();
			_equipment.RepairAll();
			SpendMoney(cost);
			_runner->repair_all(cost);
			RefreshEquipment();
		}
		else
		{
			_runner->error_message(S2C::ERR_OUT_OF_FUND);
		}
	}
}

int 
gplayer_imp::Repair(item & it, int where,int index)
{
	if(it.proc_type & item::ITEM_PROC_TYPE_UNREPAIRABLE) return -1;
	int durability;
	int max_durability;
	it.GetDurability(durability,max_durability);
	int offset = max_durability - durability;
	if( offset > 0 && max_durability > 0)
	{
		int repair_fee = world_manager::GetDataMan().get_item_repair_fee(it.type);
		size_t cost = (size_t)player_template::GetRepairCost(offset,max_durability,repair_fee);
		if(cost == 0) cost = 1;
		if(cost < _player_money) 
		{
			it.Repair();
			SpendMoney(cost);
			//�������
			_runner->repair(where,index, cost);
			RefreshEquipment();
			return 0;
		}
		else
		{
			//Ǯ����
			_runner->error_message(S2C::ERR_OUT_OF_FUND);
			return 1;
		}
	}
	return -1;
}

void 
gplayer_imp::RepairEquipment(int where, size_t index)
{
	switch(where)
	{
		case IL_INVENTORY:
			if(index < _inventory.Size())
			{
				item & it = _inventory[index];
				if(it.type != -1) Repair(it,where,index);
			}
			break;
		case IL_EQUIPMENT:
			if(index < _equipment.Size())
			{
				item & it = _equipment[index];
				if(it.type != -1) Repair(it,where,index);
			}
			break;
		case IL_TASK_INVENTORY:
			break;
	}
	return ;
}

void 
gplayer_imp::PlayerLogout(int type)
{
	if((_player_state != PLAYER_STATE_NORMAL
				&& _player_state != PLAYER_SIT_DOWN
				&& _player_state != PLAYER_STATE_BIND) 
				|| IsCombatState() || type < 0)
	{
		_runner->error_message(S2C::ERR_CANNOT_LOGOUT);
		return;
	}

	GLog::log(GLOG_INFO,"�û�%dִ�еǳ��߼�%d",_parent->ID.id, type);
	ASSERT(_offline_type == 0);
	//���������ҵĵǳ�����  ����LostConnection ����
	_team.PlayerLogout();
	Logout(type);
}

void 
gplayer_imp::Logout(int type)
{
	class UserInfoWriteBack : public GDB::Result, public abase::ASmallObject
	{
		gplayer * _player;
		int _userid;
		int _type;
	public:
		UserInfoWriteBack(gplayer * pPlayer,int type):_player(pPlayer),_userid(pPlayer->ID.id),_type(type)
		{}

		virtual void OnTimeOut()
		{
			//Ŀǰ��û�����·��ʹ�������
			GLog::log(GLOG_ERR,"����ʱ����%d���ݳ�ʱ",_userid);
			OnPutRole(1);
		}

		virtual void OnFailed()
		{	
			//�����ܵ�������� 
			GLog::log(GLOG_ERR,"����ʱ����%d����ʧ��",_userid);
			OnPutRole(2);
		}
		
		virtual void OnPutRole(int retcode)
		{
			//ASSERT(retcode == 0);
			//д����̳ɹ�
			_player->Lock();
			if(_player->ID.id !=  _userid || _player->login_state != gplayer::WAITING_LOGOUT) 
			{
				//���Դ���ֱ�ӷ���
				_player->Unlock();
				GLog::log(GLOG_INFO,"д���û�%d����״̬��һ�´��� id2:%d state:%d",_userid,_player->ID.id,_player->login_state);
				delete this;
				return ;
			}
			ASSERT(_player->imp);
			GLog::log(GLOG_INFO,"д���û�%d�������(%d)�����͵ǳ�����%d",_userid,retcode,_type);
			//��player����ɾ��
			((gplayer_imp*)(_player->imp))->SendLogoutRequest(_type, retcode);
			_player->imp->_commander->Release(); 
			_player->Unlock();
			delete this;
		}

	};
	ASSERT(_player_state == PLAYER_STATE_NORMAL || _player_state == PLAYER_DISCONNECT || _player_state == PLAYER_SIT_DOWN || _player_state == PLAYER_STATE_BIND);

	GLog::formatlog("playerlogout:roleid=%d:userid=%d:level=%d:occupation=%d:worldtag=%d:x=%f:y=%f:z=%f:playtime=%d",
			_parent->ID.id,_db_user_id,_basic.level,GetPlayerClass(),world_manager::GetWorldTag(),_parent->pos.x,_parent->pos.y,_parent->pos.z,(int)(g_timer.get_systime()-_login_timestamp));

	PlayerLeaveWorld();
	_runner->leave_world();
	
	//��Ҫ���Ƕ����߼�
	//�û���������߼�������ʼ����
	_player_state = PLAYER_WAIT_LOGOUT;
	gplayer * pPlayer = (gplayer*)_parent;
	pPlayer->login_state = gplayer::WAITING_LOGOUT;

	ClearSession();
	ClearAction();

	//�Ƚ��û���������ɾ�� ��commander->Release�л���û��ӹ��������Ƴ�
	slice * pPiece = pPlayer->pPiece;
	if(pPiece) _plane->RemovePlayer(pPlayer);
	pPlayer->pPiece = NULL;

	if(type >= 0)
	{
		//���ǳ�ʱ���̵㲻�ٱ�������ʱ��ɾ���л�������ʧ��װ��
		int tag;
		A3DVECTOR pos;
		world_manager::GetInstance()->GetLogoutPos(this,tag,pos);
		if(tag != world_manager::GetWorldTag())
		{
			_inventory.ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_equipment.ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_task_inventory.ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_trashbox.GetBackpack2().ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_trashbox.GetBackpack3().ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_trashbox.GetBackpack4().ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			_user_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_NO_SAVE);
			//ɾ���л������������filter
			_filters.ClearSpecFilter(filter::FILTER_MASK_NOSAVE);
		}
		//д�����	��û�п��Ƕ�д��д��ʧ�ܵ����
		//��ʱǿ�Ʊ���ֿ�
		_tb_change_counter = 1;
		_user_tb_change_counter = 1;
		user_save_data((gplayer*)_parent,new UserInfoWriteBack(pPlayer,type),2);
	}
	else
	{
		//���������������д�̲���
		SendLogoutRequest(-1);
		_commander->Release(); 
	}
}

void gplayer_imp::PlayerForceOffline()
{
	//���ʱ���������ڵȴ�д��������Ȼ���ܶ��ߣ���ʱ����Ȼ�����
	//��Ҫ���Ǳ�Ҫ���ͷŲ���
	if(_player_state == PLAYER_DISCONNECT) return;
	if(_parent->pPiece) _runner->leave_world();

	//���ﻹ��Ҫ��ʲô?
	
	//���û���������ɾ��
	_commander->Release(); 
}

void 
gplayer_imp::ServerShutDown()
{
	//�رյ�ԭ���Ǻ���ս��״̬
	if(_parent->b_disconnect) return;
	ActiveCombatState(false);
	LostConnection(PLAYER_OFF_LPG_DISCONNECT);
}

void 
gplayer_imp::SendLogoutRequest(int type, int retcode)
{
	gplayer * pPlayer  =(gplayer*)_parent;
	bool bRst;

	//yyb send update info
	if(_level_up) GMSV::SendPlayerInfoUpdate(pPlayer->ID.id, _basic.level);
	GMSV::SendSNSRoleBrief(pPlayer->ID.id, object_interface(this));

	switch(_offline_type)
	{
		case PLAYER_OFF_LOGOUT:
		bRst = GMSV::SendLogout(pPlayer->cs_index, pPlayer->ID.id, pPlayer->cs_sid,type>=0?type:GMSV::PLAYER_LOGOUT_FULL);
		//ASSERT(bRst);
		//���ٱ�������ˣ�����linkserver��������ʱ���ܻ��жϣ������������ķ��������ĸ�
		break;

		case PLAYER_OFF_OFFLINE:
		bRst = GMSV::SendOfflineRe(pPlayer->cs_index, pPlayer->ID.id, pPlayer->cs_sid,0);
		//ASSERT(bRst);
		//���ٱ�������ˣ�����linkserver��������ʱ���ܻ��жϣ������������ķ��������ĸ�
		break;
		
		case PLAYER_OFF_KICKOUT:
		bRst = GMSV::SendKickoutRe(pPlayer->ID.id, pPlayer->cs_sid,0);
		//ASSERT(bRst);
		//���ٱ�������ˣ�����linkserver��������ʱ���ܻ��жϣ������������ķ��������ĸ�
		break;

		case PLAYER_OFF_LPG_DISCONNECT:
		bRst = GMSV::SendDisconnect(pPlayer->cs_index,pPlayer->ID.id,pPlayer->cs_sid,0);
		break;

		case PLAYER_OFF_CHANGEDS:
		bRst = GMSV::SendPlayerChangeDSRe(retcode, pPlayer->ID.id, type);
		break;
	};
	
}

void
gplayer_imp::LostConnection(int offline_type)
{
	if(_parent->b_disconnect) return;
	_parent->b_disconnect = true;
	_offline_type = offline_type;
	//���ߣ����ݸ���״̬���в���
	switch(_player_state)
	{
		case PLAYER_STATE_MARKET:
		case PLAYER_STATE_COSMETIC:
			if(_player_state == PLAYER_STATE_MARKET) CancelPersonalMarket();
			if(_player_state == PLAYER_STATE_COSMETIC) LeaveCosmeticMode(0);
		case PLAYER_STATE_NORMAL:
		case PLAYER_SIT_DOWN:
		case PLAYER_STATE_BIND:	
			{
				if(!IsCombatState())
				{
					//��ս��״̬���ȴ��������˳������������˳���
					_player_state = PLAYER_DISCONNECT;
					_disconnect_timeout = LOGOUT_TIME_IN_NORMAL;
					//Logout(GMSV::PLAYER_LOGOUT_FULL);
				}
				else
				{
					//ս��״̬
					//��������߼�״̬
					_player_state = PLAYER_DISCONNECT;
					_disconnect_timeout = LOGOUT_TIME_IN_COMBAT;
				}
			}
			break;

		case PLAYER_STATE_TRAVEL:
		{
			//��������߼�
			_player_state = PLAYER_DISCONNECT;
			_disconnect_timeout = LOGOUT_TIME_IN_TRAVEL;
			_logout_pos_flag = 1;
			_logout_pos = _provider.pos;
			
		};
		break;


		case PLAYER_WAITING_TRADE:
			{
				//ֱ�Ӻ��Խ���
				//����logout ״̬
				GMSV::ReplyTradeRequest(_trade_obj->GetTradeID(),_parent->ID.id,
						((gplayer*)_parent)->cs_sid,false);
				FromTradeToNormal();
			}
			break;

		case PLAYER_TRADE:
			{
				//����״̬������ȡ����������
				GMSV::DiscardTrade(_trade_obj->GetTradeID(),_parent->ID.id);
				//������ߵȴ�״̬
				_player_state = PLAYER_WAIT_TRADE_COMPLETE;
				_trade_obj->SetTimeOut(10);
			}
			break;

		case PLAYER_WAITING_FACTION_TRADE:
		case PLAYER_WAIT_FACTION_TRADE_READ:
		case PLAYER_WAIT_TRADE_COMPLETE:
		case PLAYER_WAIT_TRADE_READ:
		case PLAYER_DISCONNECT:			// ����֮
		case PLAYER_WAIT_LOGOUT:	//����֮
			break;
	}
	//���ö�����˳��߼�
	_team.PlayerLostConnection();
}

int 
gplayer_imp::DisconnectMessageHandler(world * pPlane ,const MSG & msg)
{
	if(msg.message == GM_MSG_HEARTBEAT)
	{
		ASSERT(_disconnect_timeout > 0 && _disconnect_timeout < 1000);
		if(--_disconnect_timeout <= 0)
		{
			Logout(GMSV::PLAYER_LOGOUT_FULL);
			return 0;
		}
	}
	return DispatchNormalMessage(pPlane,msg);
}

int 
gplayer_imp::TakeOutItem(int item_id)
{
	int rst = _inventory.Find(0,item_id);
	if(rst >= 0)
	{
		item& it = _inventory[rst];
		UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
		
		_inventory.DecAmount(rst,1);
		_runner->player_drop_item(IL_INVENTORY,rst,item_id,1,S2C::DROP_TYPE_TAKEOUT);
	}
	return rst;
}

void gplayer_imp::TakeOutItem(int item_id, size_t count)   //lgc
{
	RemoveItems(item_id, count, S2C::DROP_TYPE_TAKEOUT, true);
}

void 
gplayer_imp::TakeOutItem(const int * id_list, size_t list_count, size_t count)
{
	for(size_t i=0; i<list_count; i++)
	{
		if(id_list[i] <= 0) continue;
		size_t tmp = RemoveItems(id_list[i], count, S2C::DROP_TYPE_TAKEOUT, true);
		ASSERT(tmp <= count);
		if(tmp == count) return;
		count -= tmp;
	}
}

bool 
gplayer_imp::CheckItemExist(int item_id, size_t count)
{
	int rst = 0;
	while((rst = _inventory.Find(rst,item_id)) >= 0)
	{
		if(_inventory[rst].count >= count) return true;
		count -= _inventory[rst].count;
		rst ++;
	}
	return false;
}

bool 
gplayer_imp::CheckItemExist(int inv_index, int item_id, size_t count)
{
	return IsItemExist(inv_index,item_id,count);
}

bool 
gplayer_imp::CheckItemExist(const int * id_list, size_t list_count, size_t count)
{
	for(size_t i=0; i<list_count; i++)
	{
		if(id_list[i] <= 0) continue;
		int tmp = GetItemCount(id_list[i]);
		if((size_t)tmp >= count) return true;
		count -= tmp;
	}
	return false;
}

void 
gplayer_imp::SendMsgToTeam(const MSG & msg, float range, bool exclude_self)
{
	ASSERT(IsInTeam());
	if(exclude_self)
		_team.SendMessage(msg,range);
	else
		_team.SendAllMessage(msg,range);
}

bool
gplayer_imp::Save(archive & ar)
{
	//���ﲻ����player_state,��Ϊ������normal����switch ״̬
	gactive_imp::Save(ar);
	ar << _player_money << _combat_timer << _reputation << _provider.id << _provider.pos << _money_capacity << _inv_level << _stall_trade_id << _stall_info << _last_move_mode << _pvp_cooldown << _security_passwd_checked << _pvp_enable_flag << _force_attack << _refuse_bless << _kill_by_player << _nonpenalty_pvp_state << _resurrect_state << _resurrect_exp_reduce << _resurrect_hp_factor << _resurrect_mp_factor << _resurrect_exp_lost_reduce << _ap_per_hit << _db_save_error << _pvp_combat_timer << _double_exp_timeout << _double_exp_mode << _rest_counter_time << _rest_time_used << _rest_time_capacity << _mafia_rest_time << _mafia_rest_counter_time << _login_timestamp << _played_time << _last_login_timestamp << _create_timestamp << _spec_task_reward << _spec_task_reward2 << _spec_task_reward_param << _spec_task_reward_mask << _db_timestamp << _db_user_id;

	ar << _mall_cash << _mall_cash_used << _mall_cash_offset << _mall_cash_add << _mall_order_id << _mall_order_id_saved << _mall_consumption << _chat_emote <<  _cheat_punish <<_cheat_mode  << _cheat_report << _wallow_level << _auto_hp_value << _auto_hp_percent << _auto_mp_value << _auto_mp_percent << _level_up /*<< _wallow_obj*/ << _profit_time << _profit_level << _profit_timestamp << _active_state_delay << _realm_level << _realm_exp << _leadership << _leadership_occupied << _world_contribution << _world_contribution_cost; 
	//��������
	ar << _username_len;
	ar.push_back(_username,_username_len);

	bool nosave;
	_inventory.Save(ar,nosave); ar << nosave;
	_equipment.Save(ar,nosave); ar << nosave;
	_task_inventory.Save(ar,nosave); ar << nosave;
	_trashbox.Save(ar);
	_user_trashbox.Save(ar);
	_team.Save(ar);
	_invade_ctrl.Save(ar);
	_breath.Save(ar);
	//_ph_control.Save(ar);

	//������� ������
	_duel.Save(ar);
	_bind_player.Save(ar);

	//������ȴ����
	_cooldown.Save(ar);

	//����������Ϣ
	PlayerTaskInterface task_if(this);
	size_t size = task_if.GetActLstDataSize(); ASSERT(size <= _active_task_list.size());
	ar << size;
	ar.push_back(task_if.GetActiveTaskList(),size);

	size = task_if.GetFnshLstDataSize(); ASSERT(size <= _finished_task_list.size());
	ar << size;
	ar.push_back(task_if.GetFinishedTaskList(),size);

	size = task_if.GetFnshTimeLstDataSize(); ASSERT(size <= _finished_time_task_list.size());
	ar << size;
	ar.push_back(task_if.GetFinishedTimeList(),size);

	size = task_if.GetFnshCntLstDataSize(); ASSERT(size <= _finish_task_count_list.size());
	ar << size;
	ar.push_back(task_if.GetFinishedCntList(),size);

	size = task_if.GetStorageTaskLstDataSize(); ASSERT(size <= _storage_task_list.size());
	ar << size;
	ar.push_back(task_if.GetStorageTaskList(),size);

	//����������
	ar << _role_reputation_uchar.size();
	ar.push_back(_role_reputation_uchar.begin(), _role_reputation_uchar.size()*sizeof(unsigned char));
	ar << _role_reputation_ushort.size();
	ar.push_back(_role_reputation_ushort.begin(), _role_reputation_ushort.size()*sizeof(unsigned short));
	ar << _role_reputation_uint.size();
	ar.push_back(_role_reputation_uint.begin(), _role_reputation_uint.size()*sizeof(unsigned int));
	
	//����·������
	size_t wp_size = 0;
	const void * buf = GetWaypointBuffer(wp_size);
	ar << wp_size;
	if(wp_size) ar.push_back(buf,wp_size);

	//���渱��key 
	ar << (int)_cur_tag_counter.size();
	ar.push_back(_cur_tag_counter.begin(),_cur_tag_counter.size() * sizeof(int)*2);
	ar.push_back(_ins_key_timer.begin(),_cur_tag_counter.size()*sizeof(int)*2);
	ar.push_back(_cur_ins_key_list.begin(),_cur_tag_counter.size() * sizeof(int)*3);
	ar.push_back(_team_ins_key_list.begin(),_cur_tag_counter.size() * sizeof(int)*3);

	//����ٱ������¼
	size_t shp_size = _mall_invoice.size();
	ar << shp_size;
	for(size_t i = 0;i < shp_size; i ++)
	{
		const netgame::mall_invoice & mi = _mall_invoice[i];
		ar << mi.order_id << mi.item_id << mi.item_count << mi.price<< mi.expire_date
		   << mi.guid1 << mi.guid2 << mi.timestamp;
	}
	

	//�������
	_petman.Save(ar);
	//����touch����
	_touch_order.Save(ar);
	//������Ʒ��
	_player_giftcard.Save(ar);
	//����ƺ�
	_player_title.Save(ar);
	//����ǩ��
	_player_dailysign.Save(ar);
    // ������Ұ�ȫ��״̬
    _player_sanctuary_check.Save(ar);

	//����С����  lgc
	_cur_elf_info >> ar;
	ar << _min_elf_status_value;
	_dividend_mall_info.Save(ar);
	ar.push_back(_equip_refine_level,sizeof(_equip_refine_level));
	ar << _soul_power << _soul_power_en << _min_addon_expire_date;
	_multi_exp_ctrl.Save(ar);
	ar.push_back(&_pet_enhance,sizeof(_pet_enhance));
	ar.push_back(&_faction_contrib,sizeof(_faction_contrib));
	ar << _faction_alliance.size();
	for(abase::hash_map<int,int>::iterator it=_faction_alliance.begin(); it!=_faction_alliance.end(); ++it)
		ar << it->first;
	ar << _faction_hostile.size();
	for(abase::hash_map<int,int>::iterator it=_faction_hostile.begin(); it!=_faction_hostile.end(); ++it)
		ar << it->first;
	ar << _congregate_req_list.size();
	for(abase::vector<congregate_req>::iterator it=_congregate_req_list.begin(); it!=_congregate_req_list.end(); ++it)
		ar << it->type << it->sponsor << it->timeout << it->world_tag << it->pos;
	_player_force.Save(ar);
	ar.push_back(&_force_ticket_info, sizeof(_force_ticket_info));
	_online_award.Save(ar);
	_player_limit.Save(ar);
	ar << _skill_attack_transmit_target << _country_expire_time << _in_central_server << _src_zoneid << _king_expire_time;
	if(_switch_additional_data)
		_switch_additional_data->SaveInstance(ar);
	else
		ar << (int)-1;
	_meridianman.Save(ar);

	_player_reincarnation.Save(ar);
	_player_fatering.Save(ar);
	//���濨���ռ���¼
	size_t gc_size = 0;
	const void * gc_data = _generalcard_collection.data(gc_size);
	ar << gc_size;
	if(gc_size) ar.push_back(gc_data, gc_size);

	_player_clock.Save(ar);
	_player_randmall.Save(ar);

	return true;
}

bool
gplayer_imp::Load(archive & ar)
{
	gactive_imp::Load(ar);
	ar >>  _player_money >> _combat_timer >> _reputation >> _provider.id >> _provider.pos >> _money_capacity >> _inv_level >> _stall_trade_id >> _stall_info >> _last_move_mode >> _pvp_cooldown >> _security_passwd_checked >> _pvp_enable_flag >> _force_attack >> _refuse_bless >> _kill_by_player >> _nonpenalty_pvp_state >> _resurrect_state >> _resurrect_exp_reduce >> _resurrect_hp_factor >> _resurrect_mp_factor >> _resurrect_exp_lost_reduce >> _ap_per_hit >> _db_save_error >> _pvp_combat_timer >> _double_exp_timeout >> _double_exp_mode >> _rest_counter_time >> _rest_time_used >> _rest_time_capacity >> _mafia_rest_time >> _mafia_rest_counter_time >> _login_timestamp >> _played_time >> _last_login_timestamp >> _create_timestamp >> _spec_task_reward >> _spec_task_reward2 >> _spec_task_reward_param >> _spec_task_reward_mask >> _db_timestamp >> _db_user_id;
	ar >> _mall_cash >> _mall_cash_used>> _mall_cash_offset >> _mall_cash_add >> _mall_order_id >> _mall_order_id_saved >> _mall_consumption >> _chat_emote >> _cheat_punish >> _cheat_mode >> _cheat_report >> _wallow_level >> _auto_hp_value >> _auto_hp_percent >> _auto_mp_value >> _auto_mp_percent >> _level_up /*>> _wallow_obj*/ >> _profit_time >> _profit_level >> _profit_timestamp >> _active_state_delay >> _realm_level >> _realm_exp >> _leadership >> _leadership_occupied >> _world_contribution >> _world_contribution_cost;

	//ȡ������
	ar >> _username_len;
	ar.pop_back(_username,_username_len);

	_inventory.Load(ar); ar >> _inv_switch_save_flag;
	_equipment.Load(ar); ar >> _eqp_switch_save_flag;
	_task_inventory.Load(ar); ar >>_tsk_switch_save_flag;
	_trashbox.Load(ar);
	_user_trashbox.Load(ar);
	_team.Load(ar);
	_invade_ctrl.Load(ar);
	_breath.Load(ar);
	//_ph_control.Load(ar);

	//�ָ�����������
	_duel.Load(ar);
	_bind_player.Load(ar);

	//�ָ���ȴ����
	_cooldown.Load(ar);
	
	//���³�ʼ����������
	PlayerTaskInterface task_if(this);
	size_t size;
	ar >> size; ASSERT(size <= _active_task_list.size());
	ar.pop_back(_active_task_list.begin(),size);

	ar >> size; ASSERT(size <= _finished_task_list.size());
	ar.pop_back(_finished_task_list.begin(),size);

	ar >> size; ASSERT(size <= _finished_time_task_list.size());
	ar.pop_back(_finished_time_task_list.begin(),size);

	ar >> size; ASSERT(size <= _finish_task_count_list.size());
	ar.pop_back(_finish_task_count_list.begin(),size);
	
	ar >> size; ASSERT(size <= _storage_task_list.size());
	ar.pop_back(_storage_task_list.begin(),size);
	task_if.InitActiveTaskList();
	
	//���³�ʼ��������
	size_t rr_size;
	ar >> rr_size; ASSERT(rr_size <= _role_reputation_uchar.size());
	ar.pop_back(_role_reputation_uchar.begin(),rr_size*sizeof(unsigned char));
	ar >> rr_size; ASSERT(rr_size <= _role_reputation_ushort.size());
	ar.pop_back(_role_reputation_ushort.begin(),rr_size*sizeof(unsigned short));
	ar >> rr_size; ASSERT(rr_size <= _role_reputation_uint.size());
	ar.pop_back(_role_reputation_uint.begin(),rr_size*sizeof(unsigned int));
	
	//���³�ʼ��λ������ 
	size_t wp_size;
	ar >> wp_size;
	if(wp_size)
	{
		SetWaypointList(ar.cur_data(),wp_size);
		ar.shift(wp_size);
	}

	//�ָ�����key 
	ar >> size;
	_cur_tag_counter.reserve(size);
	_cur_tag_counter.clear();
	for(size_t i =0; i < size; i ++)
	{
		int tag,counter;
		ar >> tag >> counter;
		_cur_tag_counter.push_back(abase::pair<int,int>(tag,counter));
	}
	_ins_key_timer.reserve(size);
	_ins_key_timer.clear();
	for(size_t i =0; i < size; i ++)
	{
		int time,state;
		ar >> time >> state;
		_ins_key_timer.push_back(abase::pair<int,int>(time,state));
	} 
	_cur_ins_key_list.reserve(size);
	_cur_ins_key_list.clear();
	for(size_t i =0; i < size; i ++)
	{
		int tag,key1,key2;
		ar >> tag >> key1 >> key2;
		_cur_ins_key_list[tag] = abase::pair<int,int>(key1,key2);
	}

	_team_ins_key_list.reserve(size);
	_team_ins_key_list.clear();
	for(size_t i =0; i < size; i ++)
	{
		int tag,key1,key2;
		ar >> tag >> key1 >> key2;
		_team_ins_key_list[tag] = abase::pair<int,int>(key1,key2);
	}

	//�ָ��ٱ������¼
	size_t shp_size;
	ar >> shp_size;
	_mall_invoice.clear();
	_mall_invoice.reserve(shp_size);
	for(size_t i = 0;i < shp_size; i ++)
	{
		int order_id;      
		int item_id;       
		int item_count;    
		int price;         
		int expire_date;   
		int guid1;
		int guid2;
		int ts;
		ar >> order_id >> item_id >> item_count >> price >> expire_date
	           >> guid1 >> guid2 >> ts;
		_mall_invoice.push_back(netgame::mall_invoice(order_id,item_id,item_count,price,expire_date, ts,guid1,guid2));
	}

	//��ȡ����
	_petman.Load(ar);
	//Touch���׶�ȡ
	_touch_order.Load(ar);
	//��Ʒ���ȡ
	_player_giftcard.Load(ar);
	//�ƺŶ�ȡ
	_player_title.Load(ar);
	//ǩ����ȡ
	_player_dailysign.Load(ar);
    // ��ȡ��Ұ�ȫ��״̬
    _player_sanctuary_check.Load(ar);

	//��ȡС���� lgc
	_cur_elf_info << ar;
	ar >> _min_elf_status_value;
	_dividend_mall_info.Load(ar);
	ar.pop_back(_equip_refine_level,sizeof(_equip_refine_level));	
	ar >> _soul_power >> _soul_power_en >> _min_addon_expire_date;
	_multi_exp_ctrl.Load(ar);
	ar.pop_back(&_pet_enhance,sizeof(_pet_enhance));
	ar.pop_back(&_faction_contrib,sizeof(_faction_contrib));

	_faction_alliance.clear();
	size_t fsize;
	ar >> fsize;
	while(fsize)
	{
		int factionid;
		ar >> factionid;
		_faction_alliance[factionid] = 1;
		--fsize;
	}
	_faction_hostile.clear();
	ar >> fsize;
	while(fsize)
	{
		int factionid;
		ar >> factionid;
		_faction_hostile[factionid] = 1;
		--fsize;
	}
	_congregate_req_list.clear();
	size_t csize;
	ar >> csize;
	while(csize)
	{
		congregate_req req;
		ar >> req.type >> req.sponsor >> req.timeout >> req.world_tag >> req.pos;
		_congregate_req_list.push_back(req);
		--csize;
	}
	_player_force.Load(ar);
	ar.pop_back(&_force_ticket_info,sizeof(_force_ticket_info));	
	_online_award.Load(ar);
	_player_limit.Load(ar);
	ar >> _skill_attack_transmit_target >> _country_expire_time >> _in_central_server >> _src_zoneid >> _king_expire_time;
	int switch_additional_data_guid;
	ar >> switch_additional_data_guid;
	if(switch_additional_data_guid >= 0)
	{
		substance * pSub = substance::LoadInstance(switch_additional_data_guid,ar);
		_switch_additional_data = substance::DynamicCast<switch_additional_data>(pSub);
		if(!_switch_additional_data)
		{
			delete pSub;
		}
	}
	_meridianman.Load(ar);

	_player_reincarnation.Load(ar);
	_player_fatering.Load(ar);
	//��ȡ�����ռ���¼
	size_t gc_size = 0;
	ar >> gc_size;
	if(gc_size)
	{
		unsigned char * gc_data = (unsigned char *)abase::fastalloc(gc_size);
		ar.pop_back(gc_data, gc_size);
		_generalcard_collection.init(gc_data, gc_size);
		abase::fastfree(gc_data, gc_size);
	}
	
	if(!_player_clock.Load(ar))  GLog::log(GLOG_ERR,"�û�%d ʱ��������תʱ���ִ���",_parent->ID.id);
	_player_randmall.Load(ar);

	return true;
}


bool 
gplayer_imp::EmbedChipToEquipment(size_t chip,size_t equip)
{
	if(chip >= _inventory.Size() || equip >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_CANNOT_EMBED);
		return false;
	}
	int type = _inventory[chip].type;
	int proc_type = _inventory[chip].proc_type;
	
	if(_inventory.EmbedItem(chip,equip))
	{
		UpdateMallConsumptionDestroying(type, proc_type, 1);
		((gplayer_dispatcher*)_runner)->embed_item(chip,equip);
		return true;
	}
	else
	{
		_runner->error_message(S2C::ERR_CANNOT_EMBED);
		return false;
	}
}

bool 
gplayer_imp::SharpenEquipment(size_t index, addon_data * addon_list, size_t count, int sharpener_level, int sharpener_gfx)
{
	if(index >= item::EQUIP_INVENTORY_COUNT) return false;
	if(addon_list == NULL || !count) return false;
	
	item & it = _equipment[index];
	if(it.type <= 0 || it.body == NULL) return false;
	
	if(world_manager::GetDataMan().get_item_level(it.type) < sharpener_level) return false;
	
	it.Deactivate(item::BODY, index, this);
	if(!it.Sharpen(addon_list, count, sharpener_gfx))
	{
		_runner->error_message(S2C::ERR_ITEM_CANNOT_SHARPEN);
		it.Activate(item::BODY, _equipment, index, this);
		return false;	
	}
	it.Activate(item::BODY, _equipment, index, this);
	int addon_expire = it.GetAddonExpireDate();
	if(addon_expire > 0) UpdateMinAddonExpireDate(addon_expire);
	PlayerGetItemInfo(IL_EQUIPMENT, index);//�ͻ���װ������
	//װ�����Ա��ˣ�ˢ��һ��װ��
	RefreshEquipment();
	CalcEquipmentInfo();
	int id = it.type | it.GetIdModify();
	_runner->equipment_info_changed(1ULL<<index, 0, &id, sizeof(id)); //�˺���ʹ����CalcEquipmentInfo�Ľ��
	IncEquipChangeFlag();
	return true;
}

int 
gplayer_imp::SpendFlyTime(int tick)
{
	item & it = _equipment[item::EQUIP_INDEX_FLYSWORD];
	if(it.type == -1 || it.body == NULL) return -1;
	return it.body->SpendFlyTime(tick);
}

int 
gplayer_imp::GetFlyTime()
{
	item & it = _equipment[item::EQUIP_INDEX_FLYSWORD];
	if(it.type == -1 || it.body == NULL) return -1;
	return it.body->GetFlyTime();
}

void 
gplayer_imp::KnockBack(const XID & target, const A3DVECTOR & source, float distance,int time,int stun_time)
{
	LeaveAbnormalState();
	//����һ������session
	session_knockback * pses = new session_knockback(this);
	pses->SetInfo(target, source, distance, time, stun_time);
	if(AddSession(pses)) StartSession();
}

void 
gplayer_imp::PullOver(const XID & target, const A3DVECTOR & source,float distance, int time)
{
	LeaveAbnormalState();
	session_pullover * pses = new session_pullover(this);
	pses->SetInfo(target, distance, time);
	if(AddSession(pses)) StartSession();
}

void 
gplayer_imp::Teleport(const A3DVECTOR & pos, int time, char mode)
{
	LeaveAbnormalState();
	session_teleport * pses = new session_teleport(this);
	pses->SetInfo(pos, time, mode);
	if(AddSession(pses)) StartSession();
}

void 
gplayer_imp::Teleport2(const A3DVECTOR & pos, int time, char mode)
{
	LeaveAbnormalState();
	session_teleport2 * pses = new session_teleport2(this);
	pses->SetInfo(pos, time, mode);
	if(AddSession(pses)) StartSession();
}

void
gplayer_imp::KnockUp(float distance,int time)
{
	LeaveAbnormalState();
	session_knockup * pses = new session_knockup(this);
	pses->SetInfo(distance, time);
	if(AddSession(pses)) StartSession();
}

bool 
gplayer_imp::DrainMana(int mana)
{
	if(_basic.mp >= mana)
	{
		_basic.mp -= mana;
		SetRefreshState();
		return true;
	}
	else
	{
		if(_basic.mp) SetRefreshState();
		_basic.mp = 0;
	}
	return false;
}

bool 
gplayer_imp::RechargeEquippedFlySword(size_t index, size_t count)
{
	if(count == 0 || index >= _inventory.Size()) return false;
	item & element = _inventory[index];
	if(element.type == -1 || element.count < count) return false;
	item & it = _equipment[item::EQUIP_INDEX_FLYSWORD];
	if(it.type == -1 || !it.body) return false;
	int element_id = element.type;
	DATA_TYPE dt;
	ELEMENT_ESSENCE * ess = (ELEMENT_ESSENCE*)world_manager::GetDataMan().get_data_ptr((unsigned int)element_id, ID_SPACE_ESSENCE,dt);
	if( !ess || dt != DT_ELEMENT_ESSENCE) return false;
	int cur_time;
	int rst = it.body->Recharge(ess->level,count,cur_time);
	if(rst > 0)
	{
		UpdateMallConsumptionDestroying(element.type, element.proc_type, rst);
		
		_inventory.DecAmount(index,rst);
		_runner->player_drop_item(IL_INVENTORY,index,element_id,rst,S2C::DROP_TYPE_RECHARGE);
		_runner->flysword_time_capacity(IL_EQUIPMENT,item::EQUIP_INDEX_FLYSWORD,cur_time);
	}
	return true;
}

bool 
gplayer_imp::RechargeFlySword(size_t element_index,size_t count,size_t fw_index,int fw_id)
{
	if(count == 0 || element_index >= _inventory.Size()
			|| fw_id <= 0
			|| fw_index >= _inventory.Size()) return false;
	item & element = _inventory[element_index];
	item & fw = _inventory[fw_index];
	if(element.type == -1 || element.count < count) return false;
	if(fw.type == -1 || !fw.body || fw.body->GetItemType() != item_body::ITEM_TYPE_FLYSWORD) return false;
	int element_id = element.type;

	DATA_TYPE dt;
	ELEMENT_ESSENCE * ess = (ELEMENT_ESSENCE*)world_manager::GetDataMan().get_data_ptr((unsigned int)element_id, ID_SPACE_ESSENCE,dt);
	if( !ess || dt != DT_ELEMENT_ESSENCE) return false;
	
	int old_stamina = fw.GetStamina();//����С��������
	int cur_time;
	int rst = fw.body->Recharge(ess->level,count,cur_time);
	if(rst > 0)
	{
		UpdateMallConsumptionDestroying(element.type, element.proc_type, rst);

		_inventory.DecAmount(element_index,rst);
		_runner->player_drop_item(IL_INVENTORY,element_index,element_id,rst,S2C::DROP_TYPE_RECHARGE);
		if(fw.body->GetItemType() == item_body::ITEM_TYPE_ELF)	//С���� , lgc
		{
			PlayerGetItemInfo(IL_INVENTORY, fw_index);
			_runner->elf_cmd_result(S2C::ELF_RECHARGE, element_id, rst, cur_time - old_stamina);
		}
		else	//�ɽ�
			_runner->flysword_time_capacity(IL_INVENTORY,fw_index,cur_time);
	}
	return true;
}

bool 
gplayer_imp::PlayerUseItem(int where, size_t inv_index, int item_type,size_t count)
{
	if(where != IL_INVENTORY && where != IL_EQUIPMENT) return false;
	if(count == 0) return false;
	item_list & inv = GetInventory(where);
	if(inv_index >= inv.Size() || item_type == -1 ||
			inv[inv_index].type != item_type) return false;
	if(!inv[inv_index].CanUse(inv.GetLocation())) return false;
	int rst = inv[inv_index].GetUseDuration();
	if(rst < 0)
	{
		return UseItem(inv,inv_index,where,item_type, count);
	}
	else
	{
		AddSession(new session_use_item(this,where,inv_index,item_type,count,rst));
		StartSession();
		return true;
	}
	return false;
}

bool 
gplayer_imp::PlayerSitDownUseItem(int where, size_t inv_index, int item_type,size_t count)
{
	if(where != IL_INVENTORY && where != IL_EQUIPMENT) return false;
	if(count == 0) return false;
	item_list & inv = GetInventory(where);
	if(inv_index >= inv.Size() || item_type == -1 ||
			inv[inv_index].type != item_type) return false;
	if(!inv[inv_index].SitDownCanUse(inv.GetLocation())) return false;
	int rst = inv[inv_index].GetUseDuration();
	if(rst >= 0) return false;
	return UseItem(inv,inv_index,where,item_type, count);
}

bool 
gplayer_imp::PlayerUseItemWithTarget(int where,size_t inv_index,int item_type,char force_attack)
{
	if(where != IL_INVENTORY && where != IL_EQUIPMENT) return false;
	XID cur_target = ((gplayer_controller*)_commander)->GetCurTarget();
	if(!cur_target.IsActive()) return false;

	item_list & inv = GetInventory(where);
	if(inv_index >= inv.Size() || item_type == -1 ||
			inv[inv_index].type != item_type) return false;
	if(!inv[inv_index].CanUseWithTarget(inv.GetLocation())) return false;
	int rst = inv[inv_index].GetUseDuration();
	if(rst < 0)
	{
		return UseItemWithTarget(inv,inv_index,where, item_type, cur_target,force_attack);
	}
	else
	{
		session_use_item_with_target * pSession = new session_use_item_with_target(this,where,inv_index,item_type,1,rst);
		pSession->SetTarget(cur_target,force_attack);
		AddSession(pSession);
		StartSession();
		return true;
	}
	return false;
}

void 
gplayer_imp::MakeDartAttack(int damage, float throw_range,attack_msg &attack,char force_attack)
{
	memset(&attack,0,sizeof(attack));
	attack.ainfo.level = _basic.level;
	attack.ainfo.team_id = -1; 
	attack.short_range = 0.f;
	attack.attack_range = throw_range;
	attack.attack_attr = attack_msg::PHYSIC_ATTACK; 
	attack.physic_damage  = (int)(damage*(0.005f*(_cur_prop.strength + _cur_prop.agility)));
	attack.attack_rate = _cur_prop.attack;
	attack.attacker_faction = GetFaction();
	attack.target_faction = GetEnemyFaction();
	attack.force_attack  = force_attack;
	attack.attacker_layer = _layer_ctrl.GetLayer();
}

int 
gplayer_imp::ThrowDart(const XID & target,int damage, float throw_range,char force_attack)
{
	if(!target.IsActive()) return -1;
	//�������Ƿ���� 
	enum
	{
		ALIVE = world::QUERY_OBJECT_STATE_ACTIVE | world::QUERY_OBJECT_STATE_ZOMBIE
	};
	world::object_info info;
	if(!_plane->QueryObject(target,info) ||
			(info.state & ALIVE) != world::QUERY_OBJECT_STATE_ACTIVE )
	{
		_runner->error_message(S2C::ERR_INVALID_TARGET);
		return -1;
	}
	
	float range = throw_range + info.body_size + _parent->body_size;
	if(info.pos.squared_distance(_parent->pos) > range * range)
	{
		//�޷����������ˣ������Զ
		_runner->error_message(S2C::ERR_OUT_OF_RANGE);
		return -3;
	}
	
	ActiveCombatState(true);
	_combat_timer = MAX_COMBAT_TIME;

	attack_msg attack;
	MakeDartAttack(damage, range,attack,force_attack);
	gplayer *pPlayer = GetParent();
	attack.ainfo.sid = pPlayer->cs_sid;
	
	//�������ӣ�����ֵ
	_team.GetTeamID(attack.ainfo.team_id, attack.ainfo.team_seq);

	//���ﲻ����Translate Msg ��Ϊ������ʵ��
	SendTo<0>(GM_MSG_ATTACK,target,0,&attack,sizeof(attack));

	//������ʱ�Ѿ���������ٴ����ã���Ҫ���±�д

	//����Ӧ�÷���ʹ�ð�������Ϣ
	return 0;
}

bool
gplayer_imp::ReturnToTown()
{
	A3DVECTOR pos;
	int world_tag;
	if(!world_manager::GetInstance()->GetTownPosition(this,_parent->pos,pos,world_tag)) return false;
	LongJump(pos,world_tag);
	return true;
}

bool 
gplayer_imp::PlayerSitDown()
{
	if(!CanSitDown()) return false;

	//���²���Ӧ�ñ���Ϊsession����������� 
	AddSession(new session_sit_down(this));
	StartSession();
	return true;
}

bool
gplayer_imp::PlayerStandUp()
{
	//������������
	//���²������ڲ�����
	if(_player_state != PLAYER_SIT_DOWN) return false;
	StandUp();
	return true;
}

bool 
gplayer_imp::CanSitDown()
{
	//Ҫ�ж��Ƿ��ܹ�����
	//if(IsCombatState()) return false;
	if(!_layer_ctrl.CanSitDown()) return false;
	if(_player_state != PLAYER_STATE_NORMAL) return false;
	if(((gplayer*)_parent)->IsInvisible())	return false;
	return true;
}

bool
gplayer_imp::SitDown()
{
	//���²������ڲ�����
	if(!CanSitDown()) return false;

	//Ҫ���뵽����״̬
	EnterStayInState();
	
	//�������µ�filter
	sit_down_filter * pFilter = new sit_down_filter(this);
	_filters.AddFilter(pFilter);

	//Ҫ��պ��������session
	ClearNextSession();
	return true;
}

void
gplayer_imp::StandUp()
{
	//���²������ڲ�����
	ASSERT(_player_state == PLAYER_SIT_DOWN);
	LeaveStayInState();
}

void gplayer_imp::SetIdleMode(bool sleep, bool stun)
{
	gactive_imp::SetIdleMode(sleep,stun);
	if(_idle_mode_flag)
	{
		//�����������session
		ClearSession();
		ClearAction();
	}
}

void gplayer_imp::SetSealMode(bool silent,bool root)
{
	gactive_imp::SetSealMode(silent,root);
	if(_seal_mode_flag)
	{
		if(root)
		{
			//������е��ƶ�����
			ClearSpecSession(act_session::SS_MASK_MOVE | act_session::SS_MASK_SITDOWN);
		}

		if(silent)
		{
			//������еĹ�������
			ClearSpecSession(act_session::SS_MASK_ATTACK | act_session::SS_MASK_SITDOWN);

			//Ӧ��ֹͣ��ǰ�Ĺ�������
			AddSession(new session_empty());
			StartSession();
		}
	}
}

void 
gplayer_imp::SetCombatState()
{
	ActiveCombatState(true);
	SetCombatTimer(MAX_COMBAT_TIME);
}

void 
gplayer_imp::AddAggroToEnemy(const XID & who,int rage)
{
	size_t count = _enemy_list.size();
	if(!count || rage <= 0) return;
	XID list[MAX_PLAYER_ENEMY_COUNT];
	ENEMY_LIST::iterator it = _enemy_list.begin(); 
	for(size_t i = 0;it != _enemy_list.end();i ++, ++it )
	{
		MAKE_ID(list[i],it->first);
	}

	msg_aggro_info_t info;
	info.source = who;
	info.aggro = rage;
	info.aggro_type = 0;
	info.faction = 0xFFFFFFFF;
	info.level = 0;

	MSG msg;
	BuildMessage(msg,GM_MSG_GEN_AGGRO,XID(-1,-1),who,_parent->pos,0,&info,sizeof(info));

	_plane->SendMessage(list, list + count, msg);
}

void 
gplayer_imp::ClearAggroToEnemy()
{
	gplayer* pPlayer = (gplayer*)_parent;
	if(!pPlayer->IsInvisible()) return;
	
	size_t count = _enemy_list.size();
	if(!count) return;
	XID list[MAX_PLAYER_ENEMY_COUNT];
	ENEMY_LIST::iterator it = _enemy_list.begin(); 
	for(size_t i = 0;it != _enemy_list.end();i ++, ++it )
	{
		MAKE_ID(list[i],it->first);
	}

	MSG msg;
	BuildMessage(msg,GM_MSG_TRY_CLEAR_AGGRO,XID(-1,-1),_parent->ID,_parent->pos,((gplayer*)_parent)->invisible_degree);

	_plane->SendMessage(list, list + count, msg);
}

bool 
gplayer_imp::CheckInvaderAttack(const XID & who)
{
	ASSERT(_invader_state != INVADER_LVL_0);
	return _invade_ctrl.IsDefender(who.id);
}

void 
gplayer_imp::PlayerRestartSession()
{
	if(_cur_session)
	{
		if(!_cur_session->RestartSession())
		{
			EndCurSession();
			StartSession();
		}
	}
}

namespace
{
	struct drop_entry
	{
		short where;
		short index;
		drop_entry(short w, short i):where(w),index(i)
		{}
	};
}

void 
gplayer_imp::DropItemOnDeath(size_t drop_count_inv,size_t drop_count_equip,const XID & spec_owner)
{
	ASSERT(drop_count_inv + drop_count_equip);
	
	abase::vector<drop_entry,abase::fast_alloc<> > inv_list;
	abase::vector<drop_entry,abase::fast_alloc<> > equip_list;
	abase::vector<drop_entry,abase::fast_alloc<> > drop_list;
	if(drop_count_inv)
	{
		inv_list.reserve(_inventory.Size() + item::EQUIP_INVENTORY_COUNT);
		for(size_t i = 0; i < _inventory.Size(); i ++)
		{
			if(_inventory[i].type == -1) continue;
			inv_list.push_back(drop_entry(IL_INVENTORY, i));
		}
		static int inventory_equipment[] = { item::EQUIP_INDEX_PROJECTILE, item::EQUIP_INDEX_RUNE_SLOT};
		for(size_t i = 0; i < sizeof(inventory_equipment) / sizeof(int); i ++)
		{
			int index = inventory_equipment[i];
			if(_equipment[index].type == -1) continue;
			inv_list.push_back(drop_entry(IL_EQUIPMENT,index));
		}
	}

	if(drop_count_equip)
	{
		equip_list.reserve(item::EQUIP_INVENTORY_COUNT);
		static int equipment_drop[] = 
		{
			item::EQUIP_INDEX_WEAPON, item::EQUIP_INDEX_HEAD, item::EQUIP_INDEX_NECK,
			item::EQUIP_INDEX_SHOULDER, item::EQUIP_INDEX_BODY, item::EQUIP_INDEX_WAIST,
			item::EQUIP_INDEX_LEG, item::EQUIP_INDEX_FOOT, item::EQUIP_INDEX_WRIST,
			item::EQUIP_INDEX_FINGER1, item::EQUIP_INDEX_FINGER2, item::EQUIP_INDEX_FLYSWORD,
			item::EQUIP_INDEX_BIBLE, item::EQUIP_INDEX_BUGLE,
		};

		for(size_t i = 0; i < sizeof(equipment_drop) / sizeof(int); i ++)
		{
			int index = equipment_drop[i];
			if(_equipment[index].type == -1) continue;
			equip_list.push_back(drop_entry(IL_EQUIPMENT,index));
		}
	}


	drop_list.reserve(drop_count_inv+drop_count_equip);

	for(size_t i = 0; i < drop_count_inv && inv_list.size();i ++)
	{
		int index = abase::Rand(0,inv_list.size()-1);
		drop_list.push_back(inv_list[index]);
		inv_list.erase_noorder(inv_list.begin() + index);
	}

	for(size_t i = 0; i < drop_count_equip && equip_list.size();i ++)
	{
		int index = abase::Rand(0,equip_list.size()-1);
		drop_list.push_back(equip_list[index]);
		equip_list.erase_noorder(equip_list.begin() + index);
	}

	enum 
	{
		NO_DROP_TYPE = item::ITEM_PROC_TYPE_NODROP | item::ITEM_PROC_TYPE_NOTHROW | item::ITEM_PROC_TYPE_NOTRADE |item::ITEM_PROC_TYPE_BIND
	};
	for(size_t i = 0; i < drop_list.size(); i ++)
	{
		drop_entry ent = drop_list[i];
		if(ent.where == IL_INVENTORY)
		{
			if(_inventory[ent.index].proc_type & NO_DROP_TYPE) continue;
			ThrowInvItem(ent.index ,MAX_ITEM_DROP_COUNT,false,S2C::DROP_TYPE_DEATH,spec_owner);
		}
		else
		{
			if(_equipment[ent.index].proc_type & NO_DROP_TYPE) continue;
			ThrowEquipItem(ent.index ,false,S2C::DROP_TYPE_DEATH,MAX_ITEM_DROP_COUNT,spec_owner);
		}
	}
}

void 
gplayer_imp::DamageItemOnDeath(bool restrict_bind, const XID & killer)
{
	abase::vector<int,abase::fast_alloc<> > damage_list;	
	damage_list.reserve(item::EQUIP_INVENTORY_COUNT);
	
	static int equipment_damage[] = 
	{
		item::EQUIP_INDEX_WEAPON, item::EQUIP_INDEX_HEAD, item::EQUIP_INDEX_NECK,
		item::EQUIP_INDEX_SHOULDER, item::EQUIP_INDEX_BODY, item::EQUIP_INDEX_WAIST,
		item::EQUIP_INDEX_LEG, item::EQUIP_INDEX_FOOT, item::EQUIP_INDEX_WRIST,
		item::EQUIP_INDEX_FINGER1, item::EQUIP_INDEX_FINGER2, item::EQUIP_INDEX_FLYSWORD,
		item::EQUIP_INDEX_BIBLE,
	};
	for(size_t i = 0; i < sizeof(equipment_damage) / sizeof(int); i ++)
	{
		int index = equipment_damage[i];
		if(_equipment[index].type == -1) continue;
		if(_equipment[index].proc_type & item::ITEM_PROC_TYPE_DAMAGED) continue;
		if(restrict_bind && !(_equipment[index].proc_type & item::ITEM_PROC_TYPE_BIND)) continue;
		damage_list.push_back(index);
	}
	
	if(damage_list.size() == 0) return;
	int damage_index = damage_list[abase::Rand(0,damage_list.size()-1)];	
	item & it = _equipment[damage_index];
	
	//����Ʒ	
	unsigned int damage_drop;
	int damange_drop_count;
	if( (damange_drop_count = world_manager::GetDataMan().get_item_damaged_drop(it.type,damage_drop)) <= 0) return;
	element_data::item_tag_t tag = {element_data::IMT_DROP,0};
	item_data * data = world_manager::GetDataMan().generate_item_for_drop(damage_drop,&tag,sizeof(tag));
	if(data == NULL) return;
	if((size_t)damange_drop_count > data->pile_limit) damange_drop_count = data->pile_limit;
	data->count = damange_drop_count;
	//���������λ��
	A3DVECTOR pos(_parent->pos);
	pos.x += abase::Rand(-0.5f,+0.5f);
	pos.z += abase::Rand(-0.5f,+0.5f);
	const grid * pGrid = &_plane->GetGrid();
	//��������˱߽磬��ô�Ͱ����Լ���λ������
	if(!pGrid->IsLocal(pos.x,pos.z))
	{
		pos.x = _parent->pos.x;
		pos.z = _parent->pos.z;
	}
	DropItemData(_plane,pos,data,(killer.IsPlayerClass()?killer:XID(-1,-1)),0,0,0);

	//�����Ʒ
	it.proc_type |= item::ITEM_PROC_TYPE_DAMAGED;
	PlayerGetItemInfo(IL_EQUIPMENT,damage_index);
	_runner->equipment_damaged(damage_index,1);
	RefreshEquipment();
	PlayerGetProperty();
}

void 
gplayer_imp::ThrowDeadDropItem()
{
	enum 
	{
		NO_DROP_TYPE = item::ITEM_PROC_TYPE_NODROP | item::ITEM_PROC_TYPE_NOTHROW | item::ITEM_PROC_TYPE_NOTRADE |item::ITEM_PROC_TYPE_BIND
	};
	for(size_t i = 0; i < _inventory.Size(); i ++)
	{
		if(_inventory[i].type == -1) continue;
		item & it = _inventory[i];
//		if(it.proc_type & NO_DROP_TYPE ) continue;
		if(!(it.proc_type & item::ITEM_PROC_TYPE_DEATH_DROP)) continue;
		ThrowInvItem(i ,it.count,false,S2C::DROP_TYPE_DEATH);
	}

	for(size_t i = 0; i < _equipment.Size(); i ++)
	{
		if(_equipment[i].type == -1) continue;
		item & it = _equipment[i];
//		if(it.proc_type & NO_DROP_TYPE ) continue;
		if(!(it.proc_type & item::ITEM_PROC_TYPE_DEATH_DROP)) continue;
		ThrowEquipItem(i ,false,S2C::DROP_TYPE_DEATH);
	}
}

void 
gplayer_imp::PlayerAssistSelect(const XID & cur_target)
{
	if(cur_target != _parent->ID && IsMember(cur_target))
	{
		SendTo<0>(GM_MSG_QUERY_SELECT_TARGET,cur_target,0);
	}
}

void
gplayer_imp::SendDataToSubscibeList()
{
	ASSERT(_subscibe_list.size() || _second_subscibe_list.size());
	packet_wrapper h1(64);
	using namespace S2C;
	//_backup_hp[1]
	CMD::Make<CMD::player_info_00>::From(h1,_parent->ID,_basic.hp,_basic,_cur_prop,IsCombatState()?1:0, GetCurTarget().id); // todo ddr
	if(_subscibe_list.size())
		send_ls_msg(_subscibe_list.begin(), _subscibe_list.end(), h1.data(),h1.size());
	if(_second_subscibe_list.size())
		send_ls_msg(_second_subscibe_list.begin(), _second_subscibe_list.end(), h1.data(),h1.size());
}

void
gplayer_imp::SendTeamDataToSubscibeList()
{
	if(!_team_visible_state_flag) return;
	packet_wrapper h1(64);
	using namespace S2C;
	//_backup_hp[1]
	CMD::Make<CMD::object_state_notify>::From(h1,_parent->ID,_visible_team_state.begin(),_visible_team_state.size(),_visible_team_state_param.begin(),_visible_team_state_param.size());
	send_ls_msg(_subscibe_list.begin(), _subscibe_list.end(), h1.data(),h1.size());
	send_ls_msg(((gplayer_dispatcher*)_runner)->_header,h1);
	if(IsInTeam()) _team.SendGroupData<0>(h1.data(),h1.size());

}

void 
gplayer_imp::SendTeamDataToMembers()
{
	ASSERT(IsInTeam());
	if(_visible_team_state.empty()) return;
	packet_wrapper h1(64);
	using namespace S2C;
	//_backup_hp[1]
	CMD::Make<CMD::object_state_notify>::From(h1,_parent->ID,_visible_team_state.begin(),_visible_team_state.size(),_visible_team_state_param.begin(),_visible_team_state_param.size());
	_team.SendGroupData<0>(h1.data(),h1.size());
}


void 
gplayer_imp::Swap(gplayer_imp * rhs)
{
	((gplayer_controller *)_commander)->LoadFrom((gplayer_controller*)rhs->_commander);
	((gplayer_dispatcher*)_runner)->LoadFrom((gplayer_dispatcher*)rhs->_runner);

#define Set(var,cls) var = cls->var

	Set(_player_money,rhs);
	Set(_combat_timer,rhs);
	Set(_reputation,rhs);
	Set(_provider,rhs);
	Set(_money_capacity,rhs);
	Set(_inv_level ,rhs);
	Set(_stall_trade_id,rhs);
	Set(_stall_info,rhs);
	Set(_last_move_mode,rhs);
	Set(_pvp_cooldown,rhs);
	Set(_security_passwd_checked,rhs);
	Set(_pvp_enable_flag,rhs);
	Set(_force_attack,rhs);
	Set(_refuse_bless,rhs);
	Set(_kill_by_player,rhs);
	Set(_nonpenalty_pvp_state,rhs);
	Set(_resurrect_exp_reduce,rhs);
	Set(_resurrect_hp_factor,rhs);
	Set(_resurrect_mp_factor,rhs);
	Set(_resurrect_exp_lost_reduce,rhs);
	Set(_resurrect_state,rhs);
	Set(_ap_per_hit,rhs);
	Set(_db_save_error,rhs);
	Set(_pvp_combat_timer,rhs);
	Set(_username_len,rhs);
	memcpy(_username,rhs->_username,sizeof(_username));
	Set(_double_exp_timeout,rhs);
	Set(_double_exp_mode,rhs);
	Set(_rest_counter_time,rhs);
	Set(_rest_time_used,rhs);
	Set(_rest_time_capacity,rhs);
	Set(_mafia_rest_time,rhs);
	Set(_mafia_rest_counter_time,rhs);
	Set(_login_timestamp,rhs);
	Set(_played_time,rhs);
	Set(_last_login_timestamp,rhs);
	Set(_create_timestamp,rhs);
	Set(_spec_task_reward,rhs);
	Set(_spec_task_reward2,rhs);
	Set(_spec_task_reward_param,rhs);
	Set(_spec_task_reward_mask,rhs);
	Set(_db_timestamp,rhs);
	Set(_mall_cash          ,rhs);
	Set(_mall_cash_used 	,rhs);
	Set(_mall_cash_offset   ,rhs);
	Set(_mall_cash_add		,rhs);
	Set(_mall_order_id      ,rhs);
	Set(_mall_order_id_saved,rhs);
	Set(_mall_consumption	,rhs);
	Set(_chat_emote,rhs);
	Set(_cheat_punish,rhs);
	Set(_cheat_mode,rhs);
	Set(_cheat_report,rhs);
	Set(_wallow_level,rhs);
	Set(_profit_time,rhs);
	Set(_profit_level,rhs);
	Set(_profit_timestamp,rhs);
	Set(_active_state_delay,rhs);
	Set(_auto_hp_value,rhs);
	Set(_auto_hp_percent,rhs);
	Set(_auto_mp_value,rhs);
	Set(_auto_mp_percent,rhs);
	Set(_inv_switch_save_flag,rhs);
	Set(_eqp_switch_save_flag,rhs);
	Set(_tsk_switch_save_flag,rhs);
	Set(_db_user_id,rhs);
	Set(_level_up,rhs);

	_mall_invoice.swap(rhs->_mall_invoice);
	_inventory.Swap(rhs->_inventory);
	_equipment.Swap(rhs->_equipment);
	_task_inventory.Swap(rhs->_task_inventory);
	_trashbox.Swap(rhs->_trashbox);
	_user_trashbox.Swap(rhs->_user_trashbox);
	_team.Swap(rhs->_team);
	_invade_ctrl.Swap(rhs->_invade_ctrl);
	_breath.Swap(rhs->_breath);
	_ph_control.Swap(rhs->_ph_control);
	_duel.Swap(rhs->_duel);
	_bind_player.Swap(rhs->_bind_player);
//	_wallow_obj.Swap(rhs->_wallow_obj);
	
	_cooldown.Swap(rhs->_cooldown);

	_task_mask = rhs->_task_mask;
	_active_task_list.swap(rhs->_active_task_list);
	_finished_task_list.swap(rhs->_finished_task_list);
	_finished_time_task_list.swap(rhs->_finished_time_task_list);
	_finish_task_count_list.swap(rhs->_finish_task_count_list);
	_storage_task_list.swap(rhs->_storage_task_list);

	_role_reputation_uchar.swap(rhs->_role_reputation_uchar);
	_role_reputation_ushort.swap(rhs->_role_reputation_ushort);
	_role_reputation_uint.swap(rhs->_role_reputation_uint);
	
	_waypoint_list.swap(rhs->_waypoint_list);

	_cur_ins_key_list.swap(rhs->_cur_ins_key_list);
	_team_ins_key_list.swap(rhs->_team_ins_key_list);
	_cur_tag_counter.swap(rhs->_cur_tag_counter);
	_ins_key_timer.swap(rhs->_ins_key_timer);

	_petman.Swap(rhs->_petman);
	_touch_order.Swap(rhs->_touch_order);	
	_player_giftcard.Swap(rhs->_player_giftcard);	
	_player_title.Swap(rhs->_player_title);
	_player_dailysign.Swap(rhs->_player_dailysign);
	_player_fatering.Swap(rhs->_player_fatering);
    _player_sanctuary_check.Swap(rhs->_player_sanctuary_check);

	Set(_cur_elf_info, rhs);	//lgc
	Set(_min_elf_status_value, rhs);
	Set(_dividend_mall_info, rhs);
	memcpy(_equip_refine_level,rhs->_equip_refine_level,sizeof(_equip_refine_level));
	Set(_soul_power,rhs);
	Set(_soul_power_en,rhs);
	Set(_min_addon_expire_date,rhs);
	Set(_multi_exp_ctrl,rhs);
	Set(_pet_enhance,rhs);
	Set(_faction_contrib,rhs);
	Set(_realm_level,rhs);
	Set(_realm_exp,rhs);
	Set(_leadership,rhs);
	Set(_leadership_occupied,rhs);
	Set(_world_contribution,rhs);
	Set(_world_contribution_cost,rhs);

	_faction_alliance.clear();
	for(abase::hash_map<int,int>::iterator it=rhs->_faction_alliance.begin(); it!=rhs->_faction_alliance.end(); ++it)
		_faction_alliance[it->first] = 1;
	_faction_hostile.clear();
	for(abase::hash_map<int,int>::iterator it=rhs->_faction_hostile.begin(); it!=rhs->_faction_hostile.end(); ++it)
		_faction_hostile[it->first] = 1;
	_congregate_req_list.swap(rhs->_congregate_req_list);
	_player_force.Swap(rhs->_player_force);
	Set(_force_ticket_info,rhs);
	_online_award.Swap(rhs->_online_award);
	_player_limit.Swap(rhs->_player_limit);
	Set(_skill_attack_transmit_target,rhs);
	Set(_country_expire_time,rhs);
	Set(_in_central_server,rhs);
	Set(_src_zoneid,rhs);
	Set(_king_expire_time,rhs);
	_switch_additional_data = rhs->_switch_additional_data;
	rhs->_switch_additional_data = NULL;
	_meridianman.Swap(rhs->_meridianman);
	_player_reincarnation.Swap(rhs->_player_reincarnation);
	_generalcard_collection.swap(rhs->_generalcard_collection);
	_player_clock.Swap(rhs->_player_clock);
	_player_randmall.Swap(rhs->_player_randmall);
#undef Set
	gactive_imp::Swap(rhs);
}

void 
gplayer_imp::QueryOtherPlayerInfo1(size_t count, int ids[])
{	
	abase::vector<int, abase::fast_alloc<> > extern_user_list;

	_runner->begin_transfer();
	gplayer_dispatcher * pRunner = (gplayer_dispatcher * )_runner;
	using namespace S2C;
	for(size_t i = 0; i < count; i ++)
	{
		int id =  ids[i];
		int index = _plane->FindPlayer(id);
		if(index < 0) 
		{
			//���������ⲿ����ѯһ��
			world::object_info info;
			if(_plane->QueryObject(XID(GM_TYPE_PLAYER,id),info))
			{
				if(info.pos.horizontal_distance(_parent->pos) < 150*150
					&& ( ((gplayer*)_parent)->anti_invisible_degree >= info.invisible_degree
						 || IsInTeam() && IsMember(XID(GM_TYPE_PLAYER,id))) )
				{
					//�ռ��ö��󣬹����淢����ѯ��Ϣ
					extern_user_list.push_back(id);
				}
			}
			continue;
		}
		gplayer * pPlayer = _plane->GetPlayerByIndex(index);
		if(pPlayer->IsActived() && pPlayer->pos.horizontal_distance(_parent->pos) < 150*150 
			&& ( ((gplayer*)_parent)->anti_invisible_degree >= pPlayer->invisible_degree 
			     || ((gplayer*)_parent)->team_id > 0 && ((gplayer*)_parent)->team_id == pPlayer->team_id) ) 
		{
			if(CMD::Make<INFO::player_info_1>::From(pRunner->_pw,pPlayer))
			{
				pRunner->_pw._counter ++;
			}
		}
	}
	_runner->end_transfer();

	//��������ҵ���Ϣ ��Щ��Ϣ�϶������ⲿ��Ϣ
	if(extern_user_list.size())
	{
		MSG msg;
		BuildMessage(msg,GM_MSG_QUERY_INFO_1,XID(-1,-1),_parent->ID,_parent->pos,pRunner->_header.cs_id,&(pRunner->_header.cs_sid),sizeof(int));
		_plane->SendPlayerMessage(extern_user_list.size(),extern_user_list.begin(),msg);
	}
}

void 
gplayer_imp::QueryNPCInfo1(size_t count, int ids[])
{	
	abase::vector<XID, abase::fast_alloc<> > extern_npc_list;
	gplayer_dispatcher * pRunner = (gplayer_dispatcher * )_runner;
	using namespace S2C;
	for(size_t i = 0; i < count; i ++)
	{
		//npc�Ĵ����ȼ�һЩ
		world::object_info info;
		XID  id(GM_TYPE_NPC,ids[i]);
		if(_plane->QueryObject(id,info))
		{
			if(info.pos.horizontal_distance(_parent->pos) < 150*150
				&& ((gplayer*)_parent)->anti_invisible_degree >= info.invisible_degree)
			{
				//�ռ��ö��󣬹����淢����ѯ��Ϣ
				extern_npc_list.push_back(id);
			}
		}
	}

	//��������ҵ���Ϣ ��Щ��Ϣ�϶������ⲿ��Ϣ
	if(extern_npc_list.size())
	{
		MSG msg;
		BuildMessage(msg,GM_MSG_QUERY_INFO_1,XID(-1,-1),_parent->ID,_parent->pos,pRunner->_header.cs_id,&(pRunner->_header.cs_sid),sizeof(int));
		_plane->SendMessage(extern_npc_list.begin(),extern_npc_list.end(),msg);
	}
}

static bool regen_addon(int item_id, addon_data & data)
{
	return world_manager::GetDataMan().generate_addon_from_rands(item_id,data.id,data) >=0;
}


void 
gplayer_imp::IdentifyItemAddon(size_t index, size_t fee)
{
	if(index >= _inventory.Size())
	{
		ASSERT(false && "ǰ��Ӧ���Ѿ�������");
		return ;
	}
	ASSERT(GetMoney() >= fee && "ǰ���Ѿ�������");
	item & it = _inventory[index];
	if(it.type ==-1 || it.body == NULL) 
	{
		_runner->error_message(S2C::ERR_ITEM_NOT_IN_INVENTORY);
		_runner->identify_result(index,1);
		return ;
	}

	if(it.body->RegenAddon(it.type,regen_addon))
	{
		//���ɳɹ������ٽ�Ǯ���������ݸ��ͻ���.....
		SpendMoney(fee);
		_runner->spend_money(fee);
		PlayerGetItemInfo(IL_INVENTORY,index);
		_runner->identify_result(index,0);
	}
	else
	{
		_runner->error_message(S2C::ERR_SERVICE_ERR_REQUEST);
		_runner->identify_result(index,2);
	}
}

bool 
gplayer_imp::RefineItemAddon(size_t index, int item_type, int rt_index)
{
	if(index >= _inventory.Size()) return false;
	item & it = _inventory[index];
	if(it.type ==-1 || it.body == NULL || item_type != it.type )  return false;

	if(rt_index >= 0 &&  (size_t)rt_index  >= _inventory.Size()) return false;

	int material_need = 0xFFFFF;
	int refine_addon = world_manager::GetDataMan().get_item_refine_addon(item_type,material_need);
	if(refine_addon <=0 || material_need <= 0) return false;

	//������ʯ�Ƿ��㹻
	int material_id = world_manager::GetDataMan().get_refine_meterial_id();
	if(!_inventory.IsItemExist(material_id, material_need)) return false;

	//�����ʵ���װ���Ƿ����
	float adjust[4] = {0,0,0,0};
	float adjust2[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	int rt_id = -1;
	if(rt_index >= 0)
	{
		rt_id = _inventory[rt_index].type;
		if(rt_id <= 0) return false;
		DATA_TYPE dt2;
		const REFINE_TICKET_ESSENCE &ess= *(const REFINE_TICKET_ESSENCE*)world_manager::GetDataMan().get_data_ptr(rt_id, ID_SPACE_ESSENCE,dt2);
		if(dt2 != DT_REFINE_TICKET_ESSENCE || &ess == NULL)
		{
			return false;
		}

		//����װ�����˺�һ
		if(ess.binding_only && !(it.proc_type & item::ITEM_PROC_TYPE_BIND)) return false;
		//����װ��Ʒ������
		if(ess.require_level_max && world_manager::GetDataMan().get_item_level(it.type) > ess.require_level_max) return false;

		float adj1 = ess.ext_succeed_prob;
		float adj2 = ess.ext_reserved_prob;
		if(adj1 < 0) adj1 = 0;
		if(adj2 < 0) adj2 = 0;
		if(adj1 > 1.0) adj1 = 1.0;
		if(adj2 > 1.0) adj2 = 1.0;
		if(adj1 != ess.ext_reserved_prob || adj2 != ess.ext_succeed_prob)
		{
			__PRINTF("ǿ��ʱ����״̬����\n");
		}

		adjust[0] = adj1;		//�ɹ�����
		adjust[2] = adj2;		//��һ������

		if(ess.fail_reserve_level )
		{
			adjust[1] = 2.0;	//ӵ������ı���ʯ
		}

		for(size_t i =0; i < 12; i ++)
		{
			adjust2[i] = ess.fail_ext_succeed_prob[i];
		}
	}
	
	
	int level_result = 0;
	int rst = it.body->RefineAddon(refine_addon, level_result,adjust,adjust2);
	if(rst != item::REFINE_CAN_NOT_REFINE)
	{
		const char * tbuf[] = {"�ɹ�", "�޷�����" , "������ʧ", "���Խ���һ��", "���Ա���", "װ������","δ֪1","δ֪2","δ֪3"};
		GLog::log(GLOG_INFO,"�û�%d������Ʒ%d[%s]������ǰ����%d ���Ļ���ʯ%d ������Ʒ%d",_parent->ID.id, item_type,tbuf[rst],level_result, material_need, rt_id);
		if(level_result >= 6)
		{
			GLog::refine(_parent->ID.id,item_type, level_result, rst, material_need);
		}
	}
	
	switch(rst)
	{
		case item::REFINE_CAN_NOT_REFINE:
		//�޷����о��������;���ʧ��  ������������κα仯 
		_runner->error_message(S2C::ERR_REFINE_CAN_NOT_REFINE);
		return true;
		break;
		

		case item::REFINE_SUCCESS:
		//�����ɹ�
		_runner->refine_result(0);
		PlayerGetItemInfo(IL_INVENTORY,index);
		break;

		case item::REFINE_FAILED_LEVEL_0:
		//����һ��ʧ�ܣ�ɾ������
		_runner->refine_result(1);
		break;

		default:
		GLog::log(GLOG_ERR,"����װ��ʱ�������쳣����%d",rst);
		case item::REFINE_FAILED_LEVEL_1:
		//��������ʧ�ܣ�ɾ�����ϣ�����һ�� �����Ѿ����Զ�����
		_runner->refine_result(2);
		PlayerGetItemInfo(IL_INVENTORY,index);
		break;

		case item::REFINE_FAILED_LEVEL_2:
		//��������ʧ�ܣ�ɾ�����ϣ������Ѿ����Զ����
		_runner->refine_result(3);
		PlayerGetItemInfo(IL_INVENTORY,index);
		break;
	}
	
	//������ɾ����Ʒ
	RemoveItems(material_id,material_need, S2C::DROP_TYPE_USE, true);

	//���ʹ���˵���ʯ����ôɾ��֮
	if(rt_index >= 0)
	{
		item& it = _inventory[rt_index];
		UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);

		_inventory.DecAmount(rt_index, 1);
		_runner->player_drop_item(IL_INVENTORY,rt_index,rt_id, 1 ,S2C::DROP_TYPE_USE);
	}
	return true;
}
#define MAFIA_PVP_LEVEL_LIMIT 30
void 
gplayer_imp::UpdateMafiaPvP(unsigned char pvp_mask)
{
	if(_basic.level < MAFIA_PVP_LEVEL_LIMIT) return; // 30��������Ҳ�����
	
	if(pvp_mask & 2) // �ܹ�����
		_enemy_faction |= FACTION_MPVP_MINE_CAR;
	else
		_enemy_faction &= ~FACTION_MPVP_MINE_CAR;

	if(pvp_mask & 4) // �ܹ�������
		_enemy_faction |= FACTION_MPVP_MINE_BASE;
	else
		_enemy_faction &= ~FACTION_MPVP_MINE_BASE;
}

void 
gplayer_imp::UpdateMafiaInfo(int id, char rank, unsigned char pvp_mask)
{
	gplayer * pPlayer = (gplayer*)_parent;
	if(pPlayer->id_mafia != id)
	{
		//������ɷ����仯 �ȼ���һ�µ�ǰ˫��ʱ��
		CalcRestTime();
		//������ɱ��˾���չ��׶�
		_faction_contrib.consume_contrib = 0;
		_faction_contrib.exp_contrib = 0;
		_faction_contrib.cumulate_contrib = 0;
		_runner->faction_contrib_notify();
		if(id == 0)
		{
			//�����ǰû�а������ͬ�˵ж�
			_faction_alliance.clear();
			_faction_hostile.clear();
			_runner->faction_relation_notify();
			GLog::log(GLOG_INFO,"role%d mafiaid%d reset to 0",pPlayer->ID.id,pPlayer->id_mafia);
		}
	}
	pPlayer->id_mafia = id;
	pPlayer->rank_mafia = rank;
	if(id)
	{
		pPlayer->object_state |= gactive_object::STATE_MAFIA;
	}
	else
	{
		pPlayer->object_state &= ~gactive_object::STATE_MAFIA;
	}

	if(pPlayer->pPiece)
	{
		_runner->mafia_info_notify();
	}

	if(pvp_mask)
		pPlayer->object_state2 |= gactive_object::STATE_MAFIA_PVP_MASK;
	else
		pPlayer->object_state2 &= ~gactive_object::STATE_MAFIA_PVP_MASK;
	
	if(pPlayer->mafia_pvp_mask != pvp_mask)
	{
		UpdateMafiaPvP(pvp_mask);
		pPlayer->mafia_pvp_mask = pvp_mask;		
		if(pPlayer->pPiece)
		{
			_runner->player_mafia_pvp_mask_notify(pvp_mask);
		}
	}
}

void 
gplayer_imp::UpdateFactionRelation(int * alliance, size_t asize, int * hostile, size_t hsize, bool notify_client)
{
	_faction_alliance.clear();
	for(size_t i=0; i<asize; i++)
		_faction_alliance[ alliance[i] ] = 1;
	_faction_hostile.clear();
	for(size_t i=0; i<hsize; i++)
		_faction_hostile[ hostile[i] ] = 1;
	if(notify_client) _runner->faction_relation_notify();
}

bool 
gplayer_imp::StartTravelSession(size_t min_time,size_t max_time, const A3DVECTOR & dest_pos,float speed,int vehicle)
{
	ASSERT(_player_state == PLAYER_STATE_NORMAL && !_parent->IsZombie());
	_player_state = PLAYER_STATE_TRAVEL;
	gplayer * pPlayer = GetParent();
	pPlayer->vehicle = vehicle  & 0xFF;
	//����ȡ��������ģʽ
	//pPlayer->object_state |= gactive_object::STATE_TRAVEL;
	_provider.pos = dest_pos;
	_provider.id = XID(-1,-1);
	return true;
}

bool 
gplayer_imp::CompleteTravel(int vehicle, const A3DVECTOR & target)
{
	ASSERT(_player_state == PLAYER_STATE_TRAVEL);
	if(_player_state != PLAYER_STATE_TRAVEL) return false;
	gplayer * pPlayer = GetParent();
	_player_state = PLAYER_STATE_NORMAL;
	//����ȡ��������ģʽ
	//	pPlayer->object_state &= ~gactive_object::STATE_TRAVEL;
	if(target.squared_distance(pPlayer->pos) > 10)
	{
		LongJump(target);
	}
	return true;
}

bool 
gplayer_imp::TestSanctuary()
{
//	if(_filters.IsFilterExist(FILTER_INDEX_PVPLIMIT)) return true;
	if(player_template::IsInSanctuary(_parent->pos))
	{
		_filters.AddFilter(new pvp_limit_filter(this,FILTER_INDEX_PVPLIMIT));
		return true;
	}
	else
	{
		GLog::log(GLOG_INFO,"�û�%dδ���밲ȫ��(%f,%f)",_parent->ID.id, _parent->pos.x,_parent->pos.z);
	}
	return false;
}

bool
gplayer_imp::TestPKProtected()
{
	//��pve���������ֲ��ܿ���pk�����Բ�Ҫ�����������
	if (_basic.level <= PVP_PROTECT_LEVEL && _invader_state == gactive_imp::INVADER_LVL_0)
	{
		//ֻ�����ֵȼ����²������û���ڷ�������״̬����Ч
		if (player_template::IsInPKProtectDomain(_parent->pos))
		{
			_filters.AddFilter(new pk_protected_filter(this,FILTER_INDEX_PK_PROTECTED));
			return true;
		}
	}
	//��ͼȥ��pk����Ч��
	_filters.RemoveFilter(FILTER_INDEX_PK_PROTECTED);
	return false;
}

void 
gplayer_imp::PlayerEnterServer()
{
	ReInit();
	((gplayer_controller*)_commander)->ReInit();

	bool inv_changed = _inventory.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool eqp_changed = _equipment.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool task_changed = _task_inventory.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool tb_changed1 = _trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool tb_changed2 = _trashbox.GetBackpack2().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool tb_changed3 = _trashbox.GetBackpack3().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool tb_changed4 = _trashbox.GetBackpack4().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);
	bool utb_changed1 = _user_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP|item::ITEM_PROC_TYPE_NO_SAVE);


	//ʹ��װ����Ч
	RefreshEquipment();
	CalcEquipmentInfo();
	if(world_manager::GetWorldLimit().nofly)
	{
		//��ͼȥ������Ч��
		_filters.RemoveFilter(FILTER_FLY_EFFECT);
	}

	if(world_manager::GetWorldLimit().nomount)
	{       
		//��ͼȥ�����Ч��
		if(_filters.IsFilterExist(FILTER_INDEX_MOUNT_FILTER))
		{
			_petman.RecallPet(this);
		}
	}
	//ĳЩ����������ʹ�ö౶����	
	if(world_manager::GetWorldLimit().no_multi_exp)
	{
		_multi_exp_ctrl.DeactivateMultiExp(this);	
	}
	
	//�����Ƿ���ˮ��
	TestUnderWater();
	
	TestSanctuary();

	if(!world_manager::GetWorldParam().pve_mode)
	{
		//�����pvp������,����Ƿ������ְ�ȫ����
		TestPKProtected();
	}

	_ph_control.Initialize(this);
	UpdatePlayerLayer();

	//��ͼ�ָ�����
	_petman.PostSwitchServer(this);

	if(_inv_switch_save_flag || inv_changed)
	{
		PlayerGetInventoryDetail(gplayer_imp::IL_INVENTORY);
	}

	if(_eqp_switch_save_flag || eqp_changed)
	{
		PlayerGetInventoryDetail(gplayer_imp::IL_EQUIPMENT);
	}

	if(_tsk_switch_save_flag || task_changed)
	{
		PlayerGetInventory(gplayer_imp::IL_TASK_INVENTORY);
	}

	if(tb_changed1 || tb_changed2)
	{
		PlayerGetTrashBoxInfo(true);	
	}

	if(tb_changed3 || tb_changed4)
	{
		PlayerGetPortableTrashBoxInfo(true);
	}

	if(utb_changed1)
	{
		PlayerGetUserTrashBoxInfo(true);	
	}
	
	PlaneEnterNotify(true);
	_runner->send_world_life_time(_plane->w_life_time);
	
	if(GetCountryId()) GMSV::CountryBattleEnterMap(_parent->ID.id, world_manager::GetWorldTag());

	PlayerTaskInterface tif(this);
	tif.PlayerSwitchScene();
	CalcProfitLevel();
	_runner->update_profit_time(S2C::CMD::player_profit_time::PLAYER_SWITCH_SERVER, _profit_time, _profit_level);
	_runner->send_timestamp(); 
	_plane->SyncPlayerWorldGen((gplayer*)_parent);
}

void 
gplayer_imp::PlayerLeaveServer()
{
	//����ʲô������
	PlaneEnterNotify(false);

	PlayerTaskInterface tif(this);
	tif.PlayerLeaveScene();
}

void 
gplayer_imp::PlayerLeaveWorld()
{
	//���뿪����ʱ �������׼����������ߵ�����Ʒ
	_task_inventory.ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack2().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack3().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_trashbox.GetBackpack4().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);
	_user_trashbox.GetBackpack1().ClearByProcType(item::ITEM_PROC_TYPE_LEAVE_DROP);

	//���ڰ��������Ʒ���е��䴦��
	for(size_t i = 0; i < _inventory.Size(); i ++)
	{
		item & it = _inventory[i];
		if(it.type == -1 || (it.proc_type & item::ITEM_PROC_TYPE_LEAVE_DROP) == 0) continue;
		ThrowInvItem(i,it.count,true,S2C::DROP_TYPE_PLAYER);
	}

	//��װ�������Ʒ���е��䴦��
	for(size_t i = 0; i < _equipment.Size(); i ++)
	{
		item & it = _equipment[i];
		if(it.type == -1 || (it.proc_type & item::ITEM_PROC_TYPE_LEAVE_DROP) == 0) continue;
		ThrowEquipItem(i,true,S2C::DROP_TYPE_PLAYER);
	}

	PlaneEnterNotify(false);
	
	if(GetCountryId()) GMSV::CountryBattleOffline(_parent->ID.id, GetCountryId());

	PlayerTaskInterface tif(this);
	tif.PlayerLeaveWorld();
}

void 
gplayer_imp::PlayerEnterParallelWorld()
{
	_petman.PostSwitchServer(this);
	PlaneEnterNotify(true);
	_runner->send_world_life_time(_plane->w_life_time);
}

void 
gplayer_imp::PlayerLeaveParallelWorld()
{
	ClearSession();
	ClearAction();
	_petman.PreSwitchServer(this);
	_plantpetman.PreSwitchServer(this);
	PlaneEnterNotify(false);
}

void 
gplayer_imp::PlaneEnterNotify(bool is_enter)
{
	//���������Ǹ�log
	if(!world_manager::GetInstance()->IsUniqueWorld())
		GLog::log(LOG_INFO,"�û�%d%s����%d", _parent->ID.id, is_enter?"����":"�뿪", world_manager::GetWorldTag());

	if(is_enter)
	{
		abase::vector<int> list;
		_plane->GetSceneServiceNpc(list);			
		_runner->send_scene_service_npc_list(list.size()/2, list.begin());
	}
	
	if(is_enter) GetCommonDataList(world_manager::GetWorldLimit().common_data_notify);
	if(!world_manager::GetWorldLimit().common_data_notify) return;
	if(is_enter)
	{
		_plane->ModifyCommonValue(COMMON_VALUE_ID_PLAYER_COUNT, 1);
		_plane->AddPlayerNode(GetParent());
	}
	else
	{
		_plane->ModifyCommonValue(COMMON_VALUE_ID_PLAYER_COUNT, -1);
		_plane->DelPlayerNode(GetParent());
	}
}

int 
gplayer_imp::CheckPlayerMove(const A3DVECTOR & offset,int mode,int use_time)
{
	if(use_time  < 100 || use_time > 1000) return -1;
//	if(!CheckMove(use_time,mode)) return -1;
	//����һ�µ�������
	int lmode = _last_move_mode;
	_last_move_mode = mode;
	float t = use_time*0.001f;
	float speed_square = 1000.f;
	float cur_speed = (GetCurSpeed() + 0.2f)*t;	//��������������ֵ
	float max_speed = ((MAX_FLIGHT_SPEED+0.2f)*(MAX_FLIGHT_SPEED+0.3f))*(t*t);

	if(!(mode & C2S::MOVE_MASK_DEAD))
	{
		FindCheater2();
	}
	
	mode = mode & 0x0F;
	/* �����ǵ�����Ѫ�ĵط�
	if(mode == C2S::MOVE_MODE_FALL || mode == C2S::MOVE_MODE_JUMP)
	{
		_fall_counter += offset.y;
	}
	else
	{
		if(mode & (C2S::MOVE_MASK_SKY|C2S::MOVE_MASK_WATER))
		{
		}
		else
		{
			if(fabs(_fall_counter) > 15)
			{
			
				//����������Ѫ
			//	int hp = (int)(fabs(_fall_counter*_fall_counter) * 10);
			//	bool b = false;
			//	SendTo<0>(GM_MSG_HURT,_parent->ID,hp,&b,sizeof(b));
			}
		}
		_fall_counter = 0;
	}
	*/
	
	if(mode == C2S::MOVE_MODE_FALL || mode ==  C2S::MOVE_MODE_FLY_FALL 
			|| mode == C2S::MOVE_MODE_JUMP || mode == C2S::MOVE_MODE_SLIDE)
	{
		//��������ͻ���ͨ������move_modeƭȡ��y����ĸ��ٶȣ�������phase_control�н�����Ծ����
		//����y�������нϸ��ٶ�
		if(offset.y > (MAX_JUMP_SPEED + 0.3f)*t) 
			return -1;
		else
			speed_square = offset.x *offset.x + offset.z*offset.z;
	}
	else
	{
		speed_square = offset.x *offset.x + offset.z*offset.z;
		float my = fabs(offset.y);
		int xmode = lmode & 0x0F;
		if(my <= cur_speed)
		{
			//ʲôҲ����
		}
		else
		if(offset.y > (MAX_JUMP_SPEED + 0.3f)*t) 
		{
			return -1;
		}
		else if(xmode == C2S::MOVE_MODE_FALL 
				|| xmode ==  C2S::MOVE_MODE_FLY_FALL
				|| xmode ==  C2S::MOVE_MODE_JUMP)
		{
			//ʲôҲ����
		}
		else if(offset.y < -(MAX_JUMP_SPEED*2.0f + 0.3f)*t)
		{
			return -1;
		}
		else
		{	
			if(speed_square > max_speed) return -1;
			//�϶�����cur_speed
			return 3;
		}
	}

	if(isnan(speed_square) || isnan(offset.y)) return -1;
	if(speed_square > max_speed)
	{
		//�������ٶ�����ޣ���Ӧ�ã�ֱ�Ӵ����乬
		return -1;
	}

	float cur_speed_square = cur_speed * cur_speed;
	float stmp = 1.f/(t*t);
	if(speed_square > cur_speed_square)
	{
		//�����ǰ�ٶȳ���������ٶȣ���ô����ʷ�е�����ٶȽ��жԱ�
		//���Ƚ��ٶ�ת����Ϊ
		if(speed_square * stmp > _speed_ctrl_factor)
		{
			__PRINTF("CBTL�û�%d����������ٶ�, speed_square:%f , _speed_ctrl_factor:%f , mode:%d time:%d cur_speed:%f sp%f\n",
					_parent->ID.id,speed_square,_speed_ctrl_factor,mode,use_time,
					cur_speed ,GetCurSpeed()
					);
			__PRINTF("CBTL�û�%dλ��(%f,%f,%f) ƫ��(%f,%f,%f)\n",
					_parent->ID.id,_parent->pos.x,_parent->pos.y,_parent->pos.z,
					offset.x,offset.y,offset.z);

			//�������������ٶȣ���ôֱ��������
			_speed_ctrl_factor = cur_speed_square*stmp;
			return -1;
		}
		return 	2;
	}
	else
	{
		//����һ�����Ŀ����ٶ�
		_speed_ctrl_factor = cur_speed_square * stmp;
	}
	return 0;
}

int gplayer_imp::TransformChatData(const void * data,size_t dsize, void * out_buffer, size_t len)
{
	if(dsize < sizeof(short)) return 0;
	int cmd = *(short*)data;
	switch(cmd)
	{
		case CHAT_C2S::CHAT_EQUIP_ITEM:
		{
			CHAT_C2S::chat_equip_item & cmd = *(CHAT_C2S::chat_equip_item *)data;
			if(sizeof(cmd) != dsize) return 0;

			item_list &inv  = GetInventory(cmd.where);
			size_t index = cmd.index;
			item_data data;
			if(inv.GetItemData(index, data) <= 0) return 0;
			packet_wrapper h1(data.content_length + sizeof(data));

			h1 << (short) CHAT_S2C::CHAT_EQUIP_ITEM << data.type << data.expire_date <<  data.proc_type  << (short)data.content_length;
			h1.push_back(data.item_content, data.content_length);

			if(len < h1.size()) return 0;
			memcpy(out_buffer, h1.data(), h1.size());
			return h1.size();
		}
		break;

		case CHAT_C2S::CHAT_GENERALCARD_COLLECTION:
		{
			CHAT_C2S::chat_generalcard_collection & cmd = *(CHAT_C2S::chat_generalcard_collection *)data;
			if(sizeof(cmd) != dsize) return 0;
		
			DATA_TYPE dt;
			const POKER_ESSENCE * ess = (const POKER_ESSENCE *)world_manager::GetDataMan().get_data_ptr(cmd.card_id, ID_SPACE_ESSENCE,dt);
			if(!ess || dt != DT_POKER_ESSENCE) return 0;
			if(!_generalcard_collection.get((size_t)ess->show_order)) return 0;

			packet_wrapper h1(8);
			h1 << (short)CHAT_S2C::CHAT_GENERALCARD_COLLECTION << cmd.card_id;

			if(len < h1.size()) return 0;
			memcpy(out_buffer, h1.data(), h1.size());
			return h1.size();
		}
		break;
	}
	return 0;
}

int
gplayer_imp::PhaseControl(const A3DVECTOR & target, float terrain_height, int mode, int use_time)
{
	return _ph_control.PhaseControl(this, target, terrain_height,mode, use_time);
}

bool 
gplayer_imp::PlayerDestroyItem(int where, size_t index, int item_type)
{
	item_list & list = GetInventory(where);
	if(index >= list.Size()) return false;
	if(list[index].type == -1 || list[index].type != item_type) return false;
	item it;
	list.Remove(index,it);
	_runner->player_drop_item(where,index,item_type,it.count,S2C::DROP_TYPE_DESTROY);
	it.Release();
	return true;
}

void 
gplayer_imp::PlayerEnablePVPState()
{
	if(!world_manager::GetWorldParam().pve_mode) return; //����PVPģʽ�������ֶ��򿪴˿��أ�ֱ�ӷ���

	if(_pvp_enable_flag) return;
	if(_basic.level <= PVP_PROTECT_LEVEL) return ; 
	GetParent()->object_state |= gactive_object::STATE_PVPMODE;
	_pvp_enable_flag = true;
	_pvp_cooldown = PVP_STATE_COOLDOWN;
	_runner->enable_pvp_state(PLAYER_PVP_CLIENT);
	_runner->player_pvp_cooldown(_pvp_cooldown);
	
}

void 
gplayer_imp::PlayerDisablePVPState()
{
	if(!world_manager::GetWorldParam().pve_mode) return; //����PVPģʽ�������ֶ��رմ˿��أ�ֱ�ӷ���
	if(!_pvp_enable_flag) return;
	if(_pvp_cooldown > 0) 
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DISABLE_PVP_STATE);
		_runner->player_pvp_cooldown(_pvp_cooldown);
		return;
	}
	if(_invader_state != INVADER_LVL_0)  
	{
		_runner->error_message(S2C::ERR_CAN_NOT_DISABLE_PVP_STATE);
		return ;
	}
	GetParent()->object_state &= ~gactive_object::STATE_PVPMODE;
	_pvp_enable_flag = false;
	_pvp_cooldown = 0;
	_runner->disable_pvp_state(PLAYER_PVP_CLIENT);
}

const A3DVECTOR & 
gplayer_imp::GetLogoutPos(int &world_tag)
{
	if(_logout_pos_flag)
	{ 
		world_tag = _logout_tag;
		return _logout_pos;
	}
	else
	{ 
		world_tag = world_manager::GetWorldTag();
		return _parent->pos;
	}
}

void 
gplayer_imp::SetCoolDownData(const void * buf, size_t size)
{
	if(size == 0) return;
	raw_wrapper ar(buf,size);
	_cooldown.Load(ar);
}

void 
gplayer_imp::GetCoolDownData(archive & ar)
{
	_cooldown.Save(ar);
}

void 
gplayer_imp::GetCoolDownDataForClient(archive & ar)
{
	_cooldown.SaveForClient(ar);
}

void 
gplayer_imp::EnhanceBreathCapacity(int value)
{
	_breath.EnhanceBreathCapacity(value);
}

void 
gplayer_imp::AdjustBreathDecPoint(int offset)
{
	_breath.AdjustPointAdjust(offset);
}

void 
gplayer_imp::ImpairBreathCapacity(int value)
{
	_breath.ImpairBreathCapacity(value);
}

void 
gplayer_imp::InjectBreath(int value) 
{
	_breath.IncBreath(value);
}

void 
gplayer_imp::EnableEndlessBreath(bool bVal)
{
	_breath.EnableEndlessBreath(bVal);
}

void 
gplayer_imp::EnableFreePVP(bool bVal)
{
	//���ﲻ���ͻ��˷�������
	_free_pvp_mode = bVal;
}

void 
gplayer_imp::SwitchFashionMode()
{
	gplayer * pPlayer = GetParent();
	bool is_fashion;
	if((is_fashion = (pPlayer->object_state & gactive_object::STATE_FASHION_MODE)))
	{
		pPlayer->object_state &= ~gactive_object::STATE_FASHION_MODE;
	}
	else
	{
		pPlayer->object_state |= gactive_object::STATE_FASHION_MODE;
	}
	_runner->player_enable_fashion_mode(!is_fashion);
}

void 
gplayer_imp::ObjReturnToTown()
{
	ReturnToTown();
}

void 
gplayer_imp::AddEffectData(short effect)
{
	gplayer * pPlayer = GetParent();
	size_t count = pPlayer->effect_count;
	if(count >= MAX_PLAYER_EFFECT_COUNT)  return;
	for(size_t i = 0; i< count; i ++)
	{
		if(pPlayer->effect_list[i] == effect) return ;
	}
	pPlayer->effect_list[count] =effect;
	pPlayer->effect_count ++;
	if(_parent->pPiece)
	{
		_runner->player_enable_effect(effect);
	}
}

void 
gplayer_imp::RemoveEffectData(short effect)
{
	gplayer * pPlayer = GetParent();
	size_t count = pPlayer->effect_count;
	for(size_t i = 0; i< count; i ++)
	{
		if(pPlayer->effect_list[i] == effect) 
		{
			if( i < count -1)
			{
				pPlayer->effect_list[i] = pPlayer->effect_list[count - 1];
			}
			pPlayer->effect_count --; 
			if(_parent->pPiece)
			{
				_runner->player_disable_effect(effect);
			}
			return ;
		}
	}
}

void 
gplayer_imp::GetPlayerCharMode(archive & ar)
{
	char data;
	gplayer * pPlayer = GetParent();
	data = (pPlayer->object_state & gactive_object::STATE_FASHION_MODE)?1:0;
	if(data)
	{
		ar << (int) PLAYER_CHAR_MODE_FASHION  << (int)1;
	}
}


void 
gplayer_imp::SetPlayerCharMode(const void * buf, size_t size)
{
	if(size < sizeof(int)*2) return ; //do nothing 
	int * data = (int *)buf;
	gplayer * pPlayer = GetParent();
	size_t count = size / sizeof(int);
	for(size_t i= 0; i < count - 1 ; i += 2)
	{
		switch (data[i])
		{
			case PLAYER_CHAR_MODE_FASHION:
				if(data[i+1]) pPlayer->object_state |=  gactive_object::STATE_FASHION_MODE;
			break;
		}
	}
}

void 
gplayer_imp::EnterCosmeticMode(unsigned short inv_index, int cos_id)
{
	ASSERT(_player_state == PLAYER_STATE_NORMAL);
	_player_state = PLAYER_STATE_COSMETIC;
	//����Ϣ���ͻ���
	_runner->begin_cosmetic(inv_index);
	GLog::log(LOG_INFO,"�û�%d��ʼ����",_parent->ID.id);
}

void 
gplayer_imp::LeaveCosmeticMode(unsigned short inv_index)
{	
	ASSERT(_player_state == PLAYER_STATE_COSMETIC);
	_player_state = PLAYER_STATE_NORMAL;
	//����Ϣ���ͻ���
	_runner->end_cosmetic(inv_index);
	GLog::log(LOG_INFO,"�û�%d��������",_parent->ID.id);
}

void 
gplayer_imp::PlayerRegionTransport(int rindex, int tag)
{
	//���������ڸ�λ���й�,���Ա�����Ҫ�Ŷӵ�����
	AddSession(new session_region_transport(this, rindex, tag));
	StartSession();
}

bool
gplayer_imp::RegionTransport(int rindex, int tag)
{
	if(_player_state != PLAYER_STATE_NORMAL)
	{
		//������״̬�������л�
		return false;
	}

	//��鴫���Ƿ����Ҫ��
	A3DVECTOR target_pos; 
	int target_tag;
	if(!city_region::GetRegionTransport(_parent->pos,tag,rindex,target_pos,target_tag))
	{
		return false;
	}

	//���Ǽ���region transport����ȴ
	
	//�ֶ�����5�����Ա�֤������뽨����
	target_pos.y += 0.05f;
	//ClearNextSession();

	//��Player���и������� 
	return LongJump(target_pos,target_tag);
}

void 
gplayer_imp::EnterResurrectReadyState(float exp_reduce,float hp_factor, float mp_factor)
{
	//������Ը����״̬ ֪ͨ�ͻ����ǣ������Լ��ı�־ �;�����ʧ ��Щ���ݻ�Ҫ����
	if(!_parent->IsZombie()) return;

	_resurrect_state = true;
	if(exp_reduce < 0.f) exp_reduce =0.f;
	if(exp_reduce >1.f)  exp_reduce = 1.f;
	if(hp_factor <0.f) hp_factor = 0.f;
	if(hp_factor >1.0f) hp_factor = 1.0f;
	if(mp_factor <0.f) mp_factor = 0.f;
	if(mp_factor >1.0f) mp_factor = 1.0f;
	if(_resurrect_exp_reduce > exp_reduce)
	{
		_resurrect_exp_reduce = exp_reduce;
		_resurrect_hp_factor = hp_factor;
		_resurrect_mp_factor = mp_factor;
	}

	if(_parent->pPiece) _runner->enable_resurrect_state(_resurrect_exp_reduce);
}

bool 
gplayer_imp::CheckCoolDown(int idx) 
{ 
	return _cooldown.TestCoolDown(idx);
}

void 
gplayer_imp::SetCoolDown(int idx, int msec) 
{ 
	if(_no_cooldown_mode && player_template::GetDebugMode())
	{
		msec = 100;
	}
	_cooldown.SetCoolDown(idx,msec);
	_runner->set_cooldown(idx, msec);
}

void 
gplayer_imp::RebuildAllInstanceData(int create_time)
{
	//�����ݣ�Ҫ��ͷ��ʼ
	const abase::vector<int> & list = world_manager::_instance_tag_list;
	_cur_ins_key_list.reserve(list.size());
	_cur_tag_counter.reserve(list.size());
	_ins_key_timer.reserve(list.size());
	for(size_t i = 0; i < list.size(); i ++)
	{
		_cur_tag_counter.push_back(abase::pair<int,int>(list[i],create_time));
		_ins_key_timer.push_back(abase::pair<int,int>(0,0));
	}
	//_ins_key_timer�� timer/��Ƕ� ����_cur_tag_counter������һһ��Ӧ
	//����timer������tag�������ϴν��븱����ʱ�� ���������Ƿ������ø���
	//�Ѿ�����������Ļ��������������룬�����������timer��ʱ���5���ӵ�����
	RebuildInstanceKey();
	//���Ƴ�����ĸ���key������ת�Ʒ�����������ʧ��
	_team_ins_key_list = _cur_ins_key_list;
}

#define CUR_INSTANCE_KEY_DATA_VERSION  (int)0
void 
gplayer_imp::InitInstanceKey(int create_time, const void * buf , size_t size)
{
	if(size < sizeof(int)*2)
	{
		RebuildAllInstanceData(create_time);
		return ;
	}

	int tag_counter = world_manager::_instance_tag_list.size();
	int * pData = (int*)buf;
	if(pData[0] != CUR_INSTANCE_KEY_DATA_VERSION || pData[1] != tag_counter)
	{
		RebuildAllInstanceData(create_time);
		return ;
	}
	if(size != sizeof(int)*2 + pData[1] *(sizeof(int) *2 + sizeof(int)*3 + sizeof(int)*2))
	{
		ASSERT(false && "���ݴ�С����ȷ");
		RebuildAllInstanceData(create_time);
		return ;
	}

	//�������ݸ�ʽ����:
	/*
	   struct
	   {
	   int version;
	   size_t count;

	   _cur_tag_counter ���� 
	   _ins_key_timer ����
	   _cur_ins_key_list ����
	   };
	 */
	size_t count = pData[1];
	pData += 2;
	_cur_tag_counter.reserve(count);
	_ins_key_timer.reserve(count);
	_cur_ins_key_list.reserve(count);

	_cur_tag_counter.clear();
	_ins_key_timer.clear();
	_cur_ins_key_list.clear();
	const abase::vector<int> & list = world_manager::_instance_tag_list;
	for(size_t i = 0;i < count ; i ++)
	{
		int tag = *pData++;
		int counter = *pData++;
		_cur_tag_counter.push_back(abase::pair<int,int>(tag,counter));
		int j = 0;
		for(j = 0; j < tag_counter; j ++)
		{
			if(list[j] == tag) break;
		}
		if(j == tag_counter)
		{
			//����Tag�����˱仯�������������ɸ���ID
			__PRINTF("����TAG���б仯���������ɸ���ID�б�\n");
			_cur_tag_counter.clear();
			RebuildAllInstanceData(create_time);
			return ;
		}
	}
	for(size_t i = 0;i < count ; i ++)
	{
		int time = *pData++;
		int state = *pData++;
		_ins_key_timer.push_back(abase::pair<int,int>(time,state));
	}
	for(size_t i = 0;i < count ; i ++)
	{
		int tag = *pData++;
		int key1= *pData++;
		int key2= *pData++;
		_cur_ins_key_list[tag] = abase::pair<int,int>(key1,key2);
	}

	//���Ƴ�����ĸ���key������ת�Ʒ�����������ʧ��
	_team_ins_key_list = _cur_ins_key_list;
}

void 
gplayer_imp::SaveInstanceKey(archive & ar)
{
	size_t count = _cur_tag_counter.size();

	ar << CUR_INSTANCE_KEY_DATA_VERSION;
	ar << count;
	ar.push_back(_cur_tag_counter.begin(), count * (sizeof(int)*2) );
	ar.push_back(_ins_key_timer.begin(), count * (sizeof(int)*2) );

	abase::static_multimap<int ,abase::pair<int,int> ,abase::default_alloc>::iterator it;
	for(it = _cur_ins_key_list.begin(); it != _cur_ins_key_list.end(); ++it)
	{
		ar << it->first << it->second.first << it->second.second;
	}
}

void
gplayer_imp::RebuildInstanceKey()
{
	_cur_ins_key_list.clear();
	for(size_t i = 0; i < _cur_tag_counter.size(); i ++)
	{
		abase::pair<int,int> & p = _cur_tag_counter[i];
		abase::pair<int,int> value = abase::pair<int,int>(_parent->ID.id, p.second++);
		_cur_ins_key_list[p.first] = value;
		_ins_key_timer[i].second = 0;
	}
}

bool 
gplayer_imp::GetInstanceKeyBuf(char * buf, size_t size)
{
	if(size != _team_ins_key_list.size() * (sizeof(int)*3)) return false;
	int * pBuf = (int*)buf;
	abase::static_multimap<int ,abase::pair<int,int> ,abase::default_alloc>::iterator it;
	for(it = _team_ins_key_list.begin(); it != _team_ins_key_list.end(); ++it)
	{
		*pBuf++ = it->first;
		*pBuf++ = it->second.first;
		*pBuf++ = it->second.second;
	}
	return true;
}

bool 
gplayer_imp::SetInstanceKeyBuf(const void * buf, size_t size)
{
	if(size != _cur_tag_counter.size() * sizeof(int)*3)
	{
		return false;
	}

	_team_ins_key_list.clear();
	int * pBuf = (int*)buf;
	size_t offset = 0;
	for(size_t i = 0; i < size; i += sizeof(int)*3,offset ++)
	{
		int tag = *pBuf++;
		int key1= *pBuf++;
		int key2= *pBuf++;
		_team_ins_key_list[tag] = abase::pair<int,int>(key1,key2);

		//�������и����Ľ����־
		_ins_key_timer[offset].second = 0;
	}
	return true;
}

int
gplayer_imp::CheckInstanceTimer(int tag)
{
	int t = g_timer.get_systime();
	for(size_t i = 0; i < _cur_tag_counter.size(); i ++)
	{
		if(_cur_tag_counter[i].first == tag)
		{
			if(_ins_key_timer[i].second == 1) return 1;
			if(t - _ins_key_timer[i].first > 3)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}
	}
	return 1;
}

void
gplayer_imp::SetInstanceTimer(int tag)
{
	int t = g_timer.get_systime();
	for(size_t i = 0; i < _cur_tag_counter.size(); i ++)
	{
		if(_cur_tag_counter[i].first == tag)
		{
			if(_ins_key_timer[i].second == 0) 
			{
				_ins_key_timer[i].first = t;
			}
			_ins_key_timer[i].second = 1;
			return ;
		}
	}
}

void 
gplayer_imp::ResetInstance(int world_tag)
{
	for(size_t i = 0; i < _cur_tag_counter.size(); i ++)
	{
		abase::pair<int,int> & p = _cur_tag_counter[i];
		if(p.first != world_tag) continue;

		if(_team.IsInTeam())
		{
			if(_team.IsLeader())
			{
				abase::pair<int,int> value = abase::pair<int,int>(_parent->ID.id, p.second++);
				_cur_ins_key_list[world_tag] = value;
				_ins_key_timer[i].second = 0;

				instance_hash_key ikey[2];
				ikey[0].key1 = _team_ins_key_list[world_tag].first;
				ikey[0].key2 = _team_ins_key_list[world_tag].second;
				ikey[1].key1 = value.first;
				ikey[1].key2 = value.second;
				_team_ins_key_list[world_tag] = value;
				MSG msg;
				BuildMessage(msg,GM_MSG_REBUILD_TEAM_INSTANCE_KEY,XID(-1,-1),_parent->ID,_parent->pos,world_tag,ikey,sizeof(ikey));
				_team.SendGroupMessage(msg);
			}
			else
			{
				instance_hash_key ikey;
				ikey.key1 = _team_ins_key_list[world_tag].first;
				ikey.key2 = _team_ins_key_list[world_tag].second;
				SendTo<0>(GM_MSG_REBUILD_TEAM_INSTANCE_KEY_REQ,_team.GetLeader(),world_tag,&ikey,sizeof(ikey));
			}		
		}
		else
		{
			abase::pair<int,int> value = abase::pair<int,int>(_parent->ID.id, p.second++);
			_cur_ins_key_list[world_tag] = value;
			_ins_key_timer[i].second = 0;
		}
		break;
	}
}

void 
gplayer_imp::CosmeticSuccess(int ticket_inv_idx, int ticket_id, int result, unsigned int crc)
{
	int rst = ticket_inv_idx;
	if(!IsItemExist(ticket_inv_idx,ticket_id,1))
	{
		rst = _inventory.Find(0,ticket_id);
	}

	if(rst >= 0)
	{
		item& it = _inventory[rst];
		UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);

		_inventory.DecAmount(rst,1);
		_runner->player_drop_item(gplayer_imp::IL_INVENTORY,rst, ticket_id, 1,S2C::DROP_TYPE_USE);
	}
	//������ȴʱ��
	SetCoolDown(COOLDOWN_INDEX_FACETICKET,FACETICKET_COOLDOWN_TIME);
	GetParent()->custom_crc = crc;
	_runner->cosmetic_success(crc);
	GLog::log(LOG_INFO,"�û�%d�������,��crc=%d,������%d��%d",_parent->ID.id,crc,rst>=0?1:0,ticket_id);
	//������ǰSession;
	EndCurSession();
	StartSession();
}

void 
gplayer_imp::SetPerHitAP(int ap_per_hit)
{
	_ap_per_hit = ap_per_hit;
}

void 
gplayer_imp::ModifyPerHitAP(int delta)
{
	_ap_per_hit += delta;
}

bool 
gplayer_imp::IsEquipWing()
{
	const item & it = _inventory[item::EQUIP_INDEX_FLYSWORD];
	if(it.type <= 0 || it.body == NULL) return false;
	return it.body->GetItemType() == item_body::ITEM_TYPE_WING;
}

void 
gplayer_imp::OnHeal(const XID & healer, int life)
{
	//���г�޵�����
	//AddAggroToEnemy(healer,life);
}

void 
gplayer_imp::Say(const char * msg,int channel)
{
	if(_parent->pPiece)
	{
		SaySomething(_plane,_parent->pPiece,msg,channel,0);
	}
}

void 
gplayer_imp::GBKSay(const char * msg, int channel)
{
	if(msg == NULL || *msg == 0) return;
	
	iconv_t cd = iconv_open("UCS2", "GBK");
	if(cd == (iconv_t)-1) return;
	char * ibuf = (char *)msg;
	size_t isize = strlen(msg);
	char * obuf = (char *)malloc(isize * 2);
	size_t osize = isize * 2;
	char * tmpbuf = obuf;
	size_t tmpsize = osize;
	if(iconv(cd, &ibuf, &isize, &tmpbuf, &tmpsize) == (size_t)(-1))
	{
		iconv_close(cd);
		free(obuf);
		return;
	}
	AutoBroadcastChatMsg(_plane, _parent->pPiece, obuf, osize-tmpsize, channel,0,0,0,0,0);
	iconv_close(cd);
	free(obuf);
}

void 
gplayer_imp::OnUseAttackRune()
{
	item& it = _equipment[item::EQUIP_INDEX_RUNE_SLOT];
	UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
	
	int id = _equipment[item::EQUIP_INDEX_RUNE_SLOT].type;
	_equipment.DecAmount(item::EQUIP_INDEX_RUNE_SLOT,1);
	_runner->player_drop_item(IL_EQUIPMENT,item::EQUIP_INDEX_RUNE_SLOT,id,1,S2C::DROP_TYPE_RUNE);
}

int 
gplayer_imp::GetLinkIndex()
{
	return GetParent()->cs_index;
}

int 
gplayer_imp::GetLinkSID()
{
	return GetParent()->cs_sid;
}

int 
gplayer_imp::SummonMonster(int mod_id, int count, const XID& target, int target_distance, int remain_time, char die_with_who, int path_id)
{
	object_interface oi(this);
	object_interface::minor_param prop;
	memset(&prop,0,sizeof(prop));
	
	prop.mob_id = mod_id;
	prop.remain_time = remain_time;
	prop.exp_factor = 1.f;
	prop.sp_factor = 1.f;
	prop.drop_rate = 1.f;
	prop.money_scale = 1.f;
	prop.spec_leader_id = XID(-1,-1);
	prop.parent_is_leader =false;
	prop.use_parent_faction = false;
	prop.die_with_who = die_with_who;
	prop.target_id = target;
	prop.path_id = path_id;
	for(int i =0; i < count; i ++)
	{
		int id = oi.CreateMinors(prop,target,target_distance);
		if(id != -1)	//����npc�ɹ����������ͺܴ���
		{
			XID xid;
			MAKE_ID(xid, id);

			msg_aggro_info_t info;
			info.source = _parent->ID;
			info.aggro = 10000;
			info.aggro_type = 0;
			info.faction = 0xFFFFFFFF;
			info.level = 0;
			
			SendTo<0>(GM_MSG_GEN_AGGRO,xid,0,&info,sizeof(info));
		}
		else if(i == 0) 
			return -1;
	}
	return 0;
}
	
int 
gplayer_imp::SummonNPC(int npc_id, int count, const XID& target, int target_distance, int remain_time)
{
	object_interface oi(this);
	object_interface::npc_param prop;
	memset(&prop,0,sizeof(prop));
	
	prop.npc_id = npc_id;
	prop.remain_time = remain_time;
	for(int i=0; i<count; i++)
	{
		int ret = oi.CreateNPC(prop,target,target_distance);
		if(ret != 0 && i == 0) return -1;
	}
	return 0;	
}

int 
gplayer_imp::SummonMine(int mine_id, int count, const XID& target, int target_distance, int remain_time)
{
	object_interface oi(this);
	object_interface::mine_param prop;
	memset(&prop,0,sizeof(prop));
	
	prop.mine_id = mine_id;
	prop.remain_time = remain_time;
	for(int i=0; i<count; i++)
	{
		int ret = oi.CreateMine(prop,target,target_distance);
		if(ret != 0 && i == 0) return -1;
	}
	return 0;	
}

bool
gplayer_imp::UseSoulItem(int type, int level, int power)
{
	return _player_fatering.OnUseSoul(type,level,power);
}

void 
gplayer_imp::IncAntiInvisiblePassive(int val)
{
	int prev = ((gplayer*)_parent)->anti_invisible_degree;
	_anti_invisible_passive += val;	
	property_policy::UpdatePlayerInvisible(this);
	int cur = ((gplayer*)_parent)->anti_invisible_degree;
	if(cur > prev && prev != 0)		//prev=0��ʾ��ʱ������ڽ��н�������ĳ�ʼ��
	{
		_petman.NotifyInvisibleData(this);
		_runner->on_inc_anti_invisible(prev,cur);
	}
	__PRINTF("�����ȼ�%d\n",cur);
}

void 
gplayer_imp::DecAntiInvisiblePassive(int val)
{
	int prev = ((gplayer*)_parent)->anti_invisible_degree;
	_anti_invisible_passive -= val;
	property_policy::UpdatePlayerInvisible(this);
	int cur = ((gplayer*)_parent)->anti_invisible_degree;
	if(cur < prev)
	{
		_petman.NotifyInvisibleData(this);
		_runner->on_dec_anti_invisible(prev,cur);
	}
	__PRINTF("�����ȼ�%d\n",cur);
}

void 
gplayer_imp::IncAntiInvisibleActive(int val)
{
	int prev = ((gplayer*)_parent)->anti_invisible_degree;
	_anti_invisible_active += val;	
	property_policy::UpdatePlayerInvisible(this);
	int cur = ((gplayer*)_parent)->anti_invisible_degree;
	if(cur > prev)
	{
		_petman.NotifyInvisibleData(this);
		_runner->on_inc_anti_invisible(prev,cur);
	}
	__PRINTF("�����ȼ�%d\n",cur);
}

void 
gplayer_imp::DecAntiInvisibleActive(int val)
{
	int prev = ((gplayer*)_parent)->anti_invisible_degree;
	_anti_invisible_active -= val;
	property_policy::UpdatePlayerInvisible(this);
	int cur = ((gplayer*)_parent)->anti_invisible_degree;
	if(cur < prev)
	{
		_petman.NotifyInvisibleData(this);
		_runner->on_dec_anti_invisible(prev,cur);
	}
	__PRINTF("�����ȼ�%d\n",cur);
}

void 
gplayer_imp::IncInvisiblePassive(int val)
{
	_invisible_passive += val;		
	gplayer * player = (gplayer*)_parent;
	if(player->IsInvisible())
	{
		//����ǰ��������״̬���������ȼ�����
		int prev_invisible_degree = player->invisible_degree;
		property_policy::UpdatePlayerInvisible(this);
		_runner->on_inc_invisible(prev_invisible_degree,player->invisible_degree);
		__PRINTF("����%d��������\n",player->invisible_degree);
		_petman.NotifyInvisibleData(this);
	}
}

void 
gplayer_imp::DecInvisiblePassive(int val)
{
	_invisible_passive -= val;
	gplayer * player = (gplayer*)_parent;
	if(player->IsInvisible())
	{
		//����ǰ��������״̬���������ȼ��½�
		int prev_invisible_degree = player->invisible_degree;
		property_policy::UpdatePlayerInvisible(this);
		_runner->on_dec_invisible(prev_invisible_degree,player->invisible_degree);
		__PRINTF("����%d��������\n",player->invisible_degree);
		_petman.NotifyInvisibleData(this);
	}
}

void 
gplayer_imp::SetInvisible(int invisible_degree)
{
	gplayer * player = (gplayer*)_parent;
	_invisible_active += invisible_degree;
	property_policy::UpdatePlayerInvisible(this);
	player->object_state |= gactive_object::STATE_INVISIBLE;	
	_runner->disappear_to_spec(player->invisible_degree);
	_runner->toggle_invisible(player->invisible_degree);
	ClearAggroToEnemy();
	_petman.NotifyInvisibleData(this);
	__PRINTF("����%d��������\n",player->invisible_degree);
}

void 
gplayer_imp::ClearInvisible()
{
	gplayer * player = (gplayer*)_parent;
	_runner->appear_to_spec(player->invisible_degree);
	_invisible_active = 0;
	property_policy::UpdatePlayerInvisible(this);
	player->object_state &= ~gactive_object::STATE_INVISIBLE;
	_runner->toggle_invisible(0);
	_petman.NotifyInvisibleData(this);
	__PRINTF("����������\n");
}

void 
gplayer_imp::SendDisappearToTeam()
{
	gplayer* player = (gplayer*)_parent;
	if(!IsInTeam() || !player->IsInvisible()) return;	
	
	packet_wrapper h1(16);
	using namespace S2C;
	CMD::Make<CMD::object_disappear>::From(h1,player);
	int count = _team.GetMemberNum();
	for(int i=0;i<count;i++)
	{
		const player_team::member_entry &ent = _team.GetMember(i);
		if(ent.id.id != _parent->ID.id && !CanSeeMe(ent.id.id))
			send_ls_msg(ent.cs_index,ent.id.id,ent.cs_sid,h1.data(),h1.size());
	}
}

void 
gplayer_imp::SendDisappearToTeamMember(int id,int cs_index,int cs_sid)
{
	gplayer* player = (gplayer*)_parent;
	if(!IsInTeam() || !player->IsInvisible()) return;	
	
	packet_wrapper h1(16);
	using namespace S2C;
	CMD::Make<CMD::object_disappear>::From(h1,player);
	if(!CanSeeMe(id))
		send_ls_msg(cs_index,id,cs_sid,h1.data(),h1.size());	
}

void 
gplayer_imp::SendAppearToTeam()
{
	gplayer* player = (gplayer*)_parent;
	if(!IsInTeam() || !player->IsInvisible()) return;	
	if(player->gm_invisible) return;

	packet_wrapper h1(32);
	using namespace S2C;
	CMD::Make<CMD::player_enter_world>::From(h1,player);
	int count = _team.GetMemberNum();
	for(int i=0;i<count;i++)
	{
		const player_team::member_entry &ent = _team.GetMember(i);
		if(ent.id.id != _parent->ID.id && !CanSeeMe(ent.id.id))
			send_ls_msg(ent.cs_index,ent.id.id,ent.cs_sid,h1.data(),h1.size());
	}
}

void 
gplayer_imp::SendAppearToTeamMember(int id,int cs_index,int cs_sid)
{
	gplayer* player = (gplayer*)_parent;
	if(!IsInTeam() || !player->IsInvisible()) return;	
	if(player->gm_invisible) return;
	
	packet_wrapper h1(32);
	using namespace S2C;
	CMD::Make<CMD::player_enter_world>::From(h1,player);
	if(!CanSeeMe(id))
		send_ls_msg(cs_index,id,cs_sid,h1.data(),h1.size());	
}

bool 
gplayer_imp::CanSeeMe(int player_id)
{
	int index = _plane->FindPlayer(player_id);
	if(index < 0)
	{
		//���ڿ������ⲿ�����,Ϊ�����,��Ϊ�������Լ�
		return false;	
	}
	gplayer* pPlayer = _plane->GetPlayerByIndex(index);
	if(pPlayer->IsActived() && pPlayer->pos.horizontal_distance(_parent->pos) < 150*150
		&& ((gplayer*)_parent)->invisible_degree <= pPlayer->anti_invisible_degree )
		return true;	
	return false;
}

void gplayer_imp::UpdateMinAddonExpireDate(int addon_expire)  
{
	//���õ�ʱ��֤ addon_expire>0
	if(_min_addon_expire_date <= 0)
		_min_addon_expire_date = addon_expire;
	else if(_min_addon_expire_date > addon_expire)
		_min_addon_expire_date = addon_expire;
}

void gplayer_imp::SetGMInvisible()
{
	if(_runner->is_gm_invisible()) return;
	if(_parent->pPiece) _runner->leave_world();
	_runner->set_gm_invisible(true);
	((gplayer*)_parent)->gm_invisible = true;
	_runner->gm_toggle_invisible(0);	//0������
	_commander->DenyCmd(controller::CMD_ATTACK);
}

void gplayer_imp::ClearGMInvisible()
{
	if(!_runner->is_gm_invisible()) return;
	_runner->set_gm_invisible(false);
	((gplayer*)_parent)->gm_invisible = false;
	_runner->appear();
	_runner->gm_toggle_invisible(1);	//1�Ƿ�����
	_commander->AllowCmd(controller::CMD_ATTACK);
}
	
bool gplayer_imp::ActivateSharpener(int id, int equip_idx)
{
	DATA_TYPE dt;
	SHARPENER_ESSENCE * ess = (SHARPENER_ESSENCE *)world_manager::GetDataMan().get_data_ptr(id,ID_SPACE_ESSENCE,dt);
	if(dt != DT_SHARPENER_ESSENCE || ess == NULL) return false;

	if(equip_idx < 0 || equip_idx >= item::EQUIP_INVENTORY_COUNT || !((1ULL<<equip_idx)&ess->equip_mask)) return false;
	
	if(ess->addon_time <= 0) return false;
	int expire_time = time(NULL) + ess->addon_time;
	
	addon_data addon_list[3];
	size_t i = 0;
	for( ; i<3 && i<sizeof(ess->addon)/sizeof(int); i++)
	{
		if(ess->addon[i] <= 0) break;
		if(!world_manager::GetDataMan().generate_addon(ess->addon[i],addon_list[i])) return false; 
		if(addon_list[i].arg[1] != 0xFFFF) return false; //������ʱЧ��������	
		addon_list[i].arg[1] = expire_time;
	}
	if(i == 0) return false;
	
	return SharpenEquipment(equip_idx,addon_list,i,ess->level,ess->gfx_index);
}

bool
gplayer_imp::SummonPet2(int pet_egg_id, int skill_level, int life_time)
{
	DATA_TYPE datatype;
	PET_EGG_ESSENCE * ess = (PET_EGG_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pet_egg_id, ID_SPACE_ESSENCE, datatype);
	if(ess == NULL || datatype != DT_PET_EGG_ESSENCE)	return false;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(ess->id_pet, ID_SPACE_ESSENCE, datatype);
	if(petess == NULL || datatype != DT_PET_ESSENCE)	return false;
	if(petess->id_type != 28752) return false;			//���Ʊ������ٻ���

	pet_data data;
	memset(&data,0,sizeof(data));
	data.honor_point = 500;								//�øж�����
	data.hunger_gauge = pet_data::HUNGER_LEVEL_1;		//����״̬����
	data.pet_tid = ess->id_pet;
	data.pet_vis_tid = 0;
	data.pet_egg_tid = ess->id;
	data.pet_class = pet_data::PET_CLASS_SUMMON;
	data.hp_factor = 0.5f;								//��Ѫ
	data.level = _basic.level;							//�ȼ�������ͬ
	data.color = 0;
	data.exp = 0;
	data.skill_point = 0;
	data.name_len = 0;
	int skill_count = 0;
	for(size_t i = 0; i < 32 && skill_count < pet_data::MAX_PET_SKILL_COUNT; i ++)
	{
		if(ess->skills[i].id_skill <= 0 
			|| ess->skills[i].level <= 0 ) continue;
		data.skills[skill_count].skill = ess->skills[i].id_skill;
		data.skills[skill_count].level = skill_level;	//ess->skills[i].level;//���＼�ܵȼ�ͬ�ٻ����ܵȼ�
		++ skill_count;
	}
	return _petman.ActivePet2(this, data, life_time, skill_level);
}

bool
gplayer_imp::SummonPlantPet(int pet_egg_id, int skill_level, int life_time, const XID & target, char force_attack)
{
	DATA_TYPE datatype;
	PET_EGG_ESSENCE * ess = (PET_EGG_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pet_egg_id, ID_SPACE_ESSENCE, datatype);
	if(ess == NULL || datatype != DT_PET_EGG_ESSENCE)	return false;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(ess->id_pet, ID_SPACE_ESSENCE, datatype);
	if(petess == NULL || datatype != DT_PET_ESSENCE)	return false;
	if(petess->id_type != 28913) return false;			//���Ʊ�����ֲ��

	pet_data data;
	memset(&data,0,sizeof(data));
	data.honor_point = 500;								//�øж�����
	data.hunger_gauge = pet_data::HUNGER_LEVEL_1;		//����״̬����
	data.pet_tid = ess->id_pet;
	data.pet_vis_tid = 0;
	data.pet_egg_tid = ess->id;
	data.pet_class = pet_data::PET_CLASS_PLANT;
	data.hp_factor = 1.f;								//��Ѫ
	data.level = _basic.level;							//�ȼ�������ͬ
	data.color = 0;
	data.exp = 0;
	data.skill_point = 0;
	data.name_len = 0;
	int skill_count = 0;
	for(size_t i = 0; i < 32 && skill_count < pet_data::MAX_PET_SKILL_COUNT; i ++)
	{
		if(ess->skills[i].id_skill <= 0 
			|| ess->skills[i].level <= 0 ) continue;
		data.skills[skill_count].skill = ess->skills[i].id_skill;
		data.skills[skill_count].level = skill_level;	//ess->skills[i].level;//���＼�ܵȼ�ͬ�ٻ����ܵȼ�
		++ skill_count;
	}
	return _plantpetman.ActivePlant(this, data, life_time, skill_level, target, force_attack);
}

bool 
gplayer_imp::CalcPetEnhance(int skill_level, extend_prop& prop, int& attack_degree, int& defend_degree, int& vigour)
{
	pet_enhance enhance;
	enhance.hp_percent 				= _pet_enhance.hp_percent + 5*skill_level;
	enhance.mp_percent 				= _pet_enhance.mp_percent + 5*skill_level;
	enhance.damage_percent 			= _pet_enhance.damage_percent + 5*skill_level;
	enhance.magic_damage_percent 	= _pet_enhance.magic_damage_percent + 5*skill_level;
	enhance.defense_percent 		= _pet_enhance.defense_percent + 5*skill_level;
	enhance.magic_defense_percent 	= _pet_enhance.magic_defense_percent + 5*skill_level;
	enhance.attack_degree_percent 	= _pet_enhance.attack_degree_percent + 5*skill_level;
	enhance.defend_degree_percent 	= _pet_enhance.defend_degree_percent + 5*skill_level;

	prop.max_hp += (int)((_base_prop.max_hp + player_template::GetVitHP(GetObjectClass(),_en_point.vit) + _en_point.max_hp) * 0.01f * enhance.hp_percent);
	prop.max_mp += (int)((_base_prop.max_mp + player_template::GetEngMP(GetObjectClass(),_en_point.eng) + _en_point.max_mp) * 0.01f * enhance.mp_percent);
	//int en_damage = (int)((_cur_item.damage_high + _en_point.damage_high + _base_prop.damage_high) * 0.01f * enhance.damage_percent);
	int en_damage = (int)((_cur_item.damage_magic_high + _en_point.magic_dmg_high + _base_prop.damage_magic_high) * 0.01f * enhance.damage_percent);
	prop.damage_low += en_damage; 
	prop.damage_high += en_damage;
	int en_magic_damage = (int)((_cur_item.damage_magic_high + _en_point.magic_dmg_high + _base_prop.damage_magic_high) * 0.01f * enhance.magic_damage_percent);
	prop.damage_magic_low += en_magic_damage;
	prop.damage_magic_high += en_magic_damage;
	prop.defense += (int)((_base_prop.defense + _en_point.defense) * 0.01f * enhance.defense_percent);
	for(size_t i = 0; i < MAGIC_CLASS; i ++)
		prop.resistance[i] += (int)((_base_prop.resistance[i] + _en_point.resistance[i]) * 0.01f * enhance.magic_defense_percent);
	attack_degree += (int)(_attack_degree * 0.01f * enhance.attack_degree_percent);
	defend_degree += (int)(_defend_degree * 0.01f * enhance.defend_degree_percent);
	vigour = GetVigour();
	return true;	
}

bool 
gplayer_imp::PetSacrifice()
{
	return _petman.PetSacrifice(this);
}

bool 
gplayer_imp::PlantSuicide(float distance, const XID & target, char force_attack)
{
	return _plantpetman.PlantSuicide(this, distance, target, force_attack);
}

void 
gplayer_imp::InjectPetHPMP(int hp, int mp)
{
	XID pet_id = _petman.GetCurPet();
	if(pet_id.IsValid())
	{
		msg_hp_mp_t data;
		data.hp = hp;
		data.mp = mp;
		SendTo<0>(GM_MSG_INJECT_HP_MP, pet_id, 0, &data, sizeof(data));
	}
}

void 
gplayer_imp::DrainPetHPMP(const XID & pet_id, int hp, int mp)
{
	if(pet_id.IsValid())
	{
		msg_hp_mp_t data;
		data.hp = hp;
		data.mp = mp;
		SendTo<0>(GM_MSG_DRAIN_HP_MP, pet_id, 0, &data, sizeof(data));
	}
}

void 
gplayer_imp::LongJumpToSpouse()
{
	//һ���򵥷��޴��Ͱ汾,û��ʹ��msg
	if(world_manager::GetWorldTag() != 1) return;
	
	int spouse_id = ((gplayer*)_parent)->spouse_id;
	if(spouse_id == 0) return;
	world::object_info info;
	if(!_plane->QueryObject(XID(GM_TYPE_PLAYER,spouse_id),info))
	{
		_runner->error_message(S2C::ERR_SPOUSE_NOT_IN_SAME_SCENE);
		return;
	}
	LongJump(info.pos);	
}

void 
gplayer_imp::WeaponDisabled(bool disable)
{
	gplayer * pPlayer = (gplayer*)_parent;
	if(disable)
	{
		//����ʧЧ
		if(pPlayer->disabled_equip_mask & item::EQUIP_MASK64_WEAPON) return;
		pPlayer->disabled_equip_mask |= item::EQUIP_MASK64_WEAPON;
		pPlayer->object_state |= gactive_object::STATE_EQUIPDISABLED;
	}
	else
	{
		//ȡ������ʧЧ
		if(!(pPlayer->disabled_equip_mask & item::EQUIP_MASK64_WEAPON)) return;
		pPlayer->disabled_equip_mask &= ~item::EQUIP_MASK64_WEAPON;
		if(!pPlayer->disabled_equip_mask)
			pPlayer->object_state &= ~gactive_object::STATE_EQUIPDISABLED;
	}
	_runner->player_equip_disabled(pPlayer->disabled_equip_mask);

	/*�˴���������ˢ��װ��,��Ϊ�˺�����filter attach/release�е��� ��ˢ��װ�����ܻ�����filter add/del,����filterǶ��
	RefreshEquipment();
	PlayerGetProperty();*/
	_need_refresh_equipment = true;
}
namespace
{
	struct invisible_collector
	{
		world * _plane;
		abase::vector<gobject *,abase::fast_alloc<> > &_list;
		float _squared_radius;
		invisible_collector(world * plane,abase::vector<gobject *,abase::fast_alloc<> > &list,float radius)
			:_plane(plane),_list(list),_squared_radius(radius*radius){}

		inline void operator()(slice *pPiece,const A3DVECTOR & pos)
		{
			if(!pPiece->player_list) return;
			pPiece->Lock();
			gplayer * pPlayer = (gplayer *)pPiece->player_list;
			while(pPlayer)
			{
				if(pos.squared_distance(pPlayer->pos) < _squared_radius && pPlayer->invisible_degree > 0)
				{
					_list.push_back(pPlayer);
				}
				pPlayer = (gplayer *)pPlayer->pNext;
			}
			pPiece->Unlock();
		}
	};
};
void 
gplayer_imp::DetectInvisible(float range)
{
	abase::vector<gobject *,abase::fast_alloc<> > list;
	invisible_collector worker(_plane,list,range);
	_plane->ForEachSlice(_parent->pos,range,worker);
	
	_runner->invisible_obj_list(list.begin(), list.size());
}

void 
gplayer_imp::ForceSelectTarget(const XID & target)
{
	((gplayer_controller*)_commander)->SelectTarget(target.id);
}

void 
gplayer_imp::ExchangePosition(const XID & target)
{
	if(!target.IsPlayer() || target == _parent->ID) return;
	world::object_info info;
	if(!_plane->QueryObject(target, info)) return;
	if(info.state & world::QUERY_OBJECT_STATE_ZOMBIE) return;
	if(info.pos.squared_distance(_parent->pos) >= 25.f * 25.f) return;

	MSG msg;
	BuildMessage(msg,GM_MSG_EXCHANGE_POS,XID(-1,-1),_parent->ID,_parent->pos,0,&info.pos,sizeof(info.pos));
	XID list[2] = {_parent->ID, target};
	_plane->SendMessage(list, list + 2, msg);
}

void 
gplayer_imp::SetSkillAttackTransmit(const XID & target)
{
	if(_skill_attack_transmit_target != _parent->ID)
		_skill_attack_transmit_target = target;
}

void 
gplayer_imp::QueryOtherInventory(const XID& target)
{
	if(!target.IsPlayer() || target.id == _parent->ID.id) return;
	
	world::object_info info;
	if(!_plane->QueryObject(target,info)) return;
	if(_parent->pos.squared_distance(info.pos) > 30.f*30.f) return;
	
	msg_player_t data;
	data.id 		= _parent->ID.id;
	data.cs_index 	= ((gplayer*)_parent)->cs_index;
	data.cs_sid 	= ((gplayer*)_parent)->cs_sid;
	SendTo<0>(GM_MSG_QUERY_INVENTORY_DETAIL,target,0,&data,sizeof(data));
}

void 
gplayer_imp::SetPlayerLimit(int index, bool b)
{ 
	if(_player_limit.SetLimit(index, b))
		_runner->set_player_limit(index, b);
}

bool 
gplayer_imp::GetPlayerLimit(int index)
{ 
	return _player_limit.GetLimit(index);
}

void
gplayer_imp::EnterNonpenaltyPVPState(bool b)
{
	_nonpenalty_pvp_state = b;
	_runner->enter_nonpenalty_pvp_state(b ? 1 : 0);
}

void gplayer_imp::SendAllData(bool detail_inv, bool detail_equip, bool detail_task)
{
	PlayerGetProperty();
	if(detail_inv)
		PlayerGetInventoryDetail(IL_INVENTORY);
	else
		PlayerGetInventory(IL_INVENTORY);

	if(detail_equip)
		PlayerGetInventoryDetail(IL_EQUIPMENT);
	else
		PlayerGetInventory(IL_EQUIPMENT);

	if(detail_task)
		PlayerGetInventoryDetail(IL_TASK_INVENTORY);
	else
		PlayerGetInventory(IL_TASK_INVENTORY);
	//���������ֿ������
	PlayerGetPortableTrashBoxInfo(true);

	/*
	   {
	   time_t l_time;
	   time_t h_time;
	   int ptime;
	   _wallow_obj.GetTimeLeft(&l_time, &h_time,&ptime);
	   _runner->player_wallow_info(_wallow_level, ptime,l_time, h_time);
	   }*/

	_runner->get_player_money(GetMoney(),_money_capacity);
	_runner->get_skill_data();
	_runner->trashbox_passwd_state(_trashbox.HasPassword());
	_runner->get_reputation(GetReputation());
	_runner->player_waypoint_list(_waypoint_list.begin(),_waypoint_list.size());
	_runner->pet_room_capacity(_petman.GetAvailPetSlot());
	_runner->player_pvp_mode();
	_petman.ClientGetPetRoom(this);
	_runner->send_cooldown_data();
	_runner->player_pvp_cooldown(_pvp_cooldown);
	_invade_ctrl.ClientGetPariahTime();
	_runner->player_cash(GetMallCash());
	_runner->player_dividend(_dividend_mall_info.GetDividend());
	_runner->send_timestamp(); 
	_runner->available_double_exp_time();
	_runner->enable_double_exp_time(_double_exp_mode, _double_exp_timeout);
	_multi_exp_ctrl.NotifyClientInfo(this);
	_multi_exp_ctrl.NotifyClientState(this);
	_runner->faction_contrib_notify();
	_runner->faction_relation_notify();
	_player_force.NotifyClient();
	_online_award.NotifyClientAllData(this);
	_runner->update_profit_time(S2C::CMD::player_profit_time::PLAYER_ONLINE, _profit_time, _profit_level);
	_runner->self_country_notify(GetCountryId());
	if(_filters.IsFilterExist(FILTER_DEFENSE_RUNE))
	{
		_runner->defense_rune_enabled(base_defense_rune_filter::RUNE_TYPE_DEFENSE,1);
	}
	if(_filters.IsFilterExist(FILTER_RESISTANCE_RUNE))
	{
		_runner->defense_rune_enabled(base_defense_rune_filter::RUNE_TYPE_RESISTANCE,1);
	}
	_meridianman.NotifyMeridianData(this);
	_player_dailysign.ClientSync(player_dailysign::SYNC4INIT);
	_player_fatering.NotifyClientRefresh();
	_runner->self_king_notify(GetParent()->IsKing() ? 1 : 0, GetKingExpireTime());
	world_manager::GetUniqueDataMan().OnRoleLogin((gplayer*)_parent);
		
	_player_reincarnation.ClientGetTome();
	_runner->realm_exp_receive(_realm_exp,0);
	_runner->player_leadership(_leadership,0);
	_runner->player_world_contribution(_world_contribution,0,_world_contribution_cost);
	size_t gc_size = 0;
	const void * gc_data = _generalcard_collection.data(gc_size);
	_runner->generalcard_collection_data(gc_data, gc_size);
	_player_instance_reenter.NotifyClient(this);
	_runner->get_task_data(); //���ָ��ͻ����ж�Ϊ�������ݷ����ָʾ
}

size_t 
gplayer_imp::OI_GetInventorySize()
{
	return _inventory.Size();
}

size_t
gplayer_imp::OI_GetEmptySlotSize()
{
	return _inventory.GetEmptySlotCount();
}

int 
gplayer_imp::OI_GetInventoryDetail(GDB::itemdata * list, size_t size)
{
	return _inventory.GetDBData(list,size);
}

int 
gplayer_imp::OI_GetEquipmentDetail(GDB::itemdata * list, size_t size) 
{ 
	return _equipment.GetDBData(list,size);
}

size_t 
gplayer_imp::OI_GetEquipmentSize() 
{ 
	return _equipment.Size();
}

int 
gplayer_imp::TradeLockPlayer(int get_mask, int put_mask)
{
	//��������Ĭ��tradeidΪ0 
	return StartFactionTrade(0,get_mask,put_mask,true)?0:-1;
}

int 
gplayer_imp::TradeUnLockPlayer()
{
	if(_player_state != PLAYER_WAITING_FACTION_TRADE) return -1;
	FromFactionTradeToNormal();
	return 0;
}


void 
gplayer_imp::GainMoneyWithDrop(size_t inc,bool write_log)
{
	size_t newmoney = _player_money + inc;
	ASSERT((int)_player_money >= 0);
	if((int)newmoney < (int)_player_money || newmoney > _money_capacity)
	{
		//���������
		newmoney = (size_t)-1;
		size_t delta = _money_capacity - _player_money;
		size_t drop = inc - delta;
		_player_money = _money_capacity; 
		DropMoneyItem(_plane,_parent->pos,drop,_parent->ID,0,0,_parent->ID.id);
		//����������ζ���¼��־�� ��Ϊ�е������
		GLog::log(GLOG_INFO,"�û�%d�õ���Ǯ%d,���е���%d",_parent->ID.id,inc,drop);

	}
	else
	{
		//����������������˲���¼��־����ô�Ͳ���¼��
		if(write_log) GLog::log(GLOG_INFO,"�û�%d�õ���Ǯ%d",_parent->ID.id,inc);
		_player_money = newmoney;
	}
}
	
void 
gplayer_imp::GainMoney(size_t inc)
{
	size_t newmoney = _player_money + inc;
	ASSERT((int)_player_money >= 0);
	if((int)newmoney < (int)_player_money || newmoney > _money_capacity)
	{
		//�����������Ǯ
		return ;
	}
	else
	{
		GLog::log(GLOG_INFO,"�û�%d�õ���Ǯ%d",_parent->ID.id,inc);
		_player_money = newmoney;
	}
}

void 
gplayer_imp::SpendMoney(size_t delta,bool write_log)
{
	ASSERT(delta <= _player_money);
	_player_money -= delta;
	if(write_log) GLog::log(GLOG_INFO,"�û�%d������Ǯ%d",_parent->ID.id,delta);
}

bool 
gplayer_imp::CheckIncMoney(size_t inc)
{
	size_t newmoney = _player_money + inc;
	if((int)newmoney < (int)_player_money || newmoney > _money_capacity)
	{
		//���
		return false ;
	}
	else
	{
		return true;
	}
}

bool 
gplayer_imp::CheckDecMoney(size_t inc)
{
	return _player_money >= inc;
}

void 
gplayer_imp::LeaveDoubleExpMode()
{
	_double_exp_mode = 0;
	_double_exp_timeout = 0;
	_runner->enable_double_exp_time(0, 0);
}

int
gplayer_imp::EnterDoubleExpMode(int time_mode, int timeout)
{
	int t = g_timer.get_systime();
	if(_double_exp_mode)
	{
		_double_exp_mode = time_mode;
		_double_exp_timeout += timeout;
		if(_double_exp_timeout - t > MAX_DOUBLE_EXP_TIME)
		{
			_double_exp_timeout = t + MAX_DOUBLE_EXP_TIME;
		}
	}
	else
	{
		_double_exp_mode = time_mode;
		_double_exp_timeout = t + timeout;
	}
	_runner->enable_double_exp_time(_double_exp_mode, _double_exp_timeout);
	return _double_exp_timeout - t;
}

bool
gplayer_imp::ActiveDoubleExpTime(int time)
{
	int cur_t = g_timer.get_systime();
	if(_double_exp_mode)
	{
		//���������˫��ʱ����ʧ��
		if(_double_exp_timeout + time > cur_t + MAX_DOUBLE_EXP_TIME)
		{
			return false;
		}
	}

	//�����Ѿ����ٸ���rest time�� ��Ϊ����ʹ����
	
	int t = EnterDoubleExpMode(1,time);
	GLog::log(GLOG_INFO,"�û�%d����˫������ʱ��%d��,˫��ʱ���ά��%dСʱ%02d��%02d��,ʣ�����˫��ʱ��%d�룬���а���˫������ʱ��%d��",
			_parent->ID.id,time,t/3600, (t/60)%60,t%60,GetDoubleExpAvailTime(),_mafia_rest_time);
	_runner->available_double_exp_time();
	return true;
}

bool 
gplayer_imp::ItemToMoney(size_t inv_index, int type, size_t count, int price)
{
	if(inv_index >= _inventory.Size()) return false;
	const item & it = _inventory[inv_index];
	if(it.type == -1 || it.type != type || it.count < count) return false;
	if(it.proc_type & item::ITEM_PROC_TYPE_NOSELL) return false;
	if(IsItemForbidSell(type))
	{
		_runner->error_message(S2C::ERR_ITEM_FORBID_SELL);
		return false;
	}

	if(GetMoney() >= _money_capacity) 
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	int real_price = GetItemRealTimePrice(it);
	if(price != real_price)
	{
		_runner->error_message(S2C::ERR_ITEM_PRICE_MISMATCH);
		return false;
	}
	float incmoney = (float)real_price * count;
	int durability, max_durability;
	it.GetDurability(durability,max_durability);
	if(durability < max_durability && max_durability > 0)
	{
		incmoney = (incmoney * durability) / max_durability;
	}
	if(incmoney < 0) incmoney = 0;
	//if(incmoney >1e11) incmoney = 0;
	if(incmoney > _money_capacity)
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}
	size_t imoney = (size_t)(incmoney+0.5f);

	if(!CheckIncMoney(imoney))
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	GLog::log(GLOG_INFO,"�û�%d����%d��%d",_parent->ID.id,count,type);
	//��ȥ��Ʒ������Ǯ��
	UpdateMallConsumptionDestroying(it.type, it.proc_type, count);
	_inventory.DecAmount(inv_index,count);
	size_t nmoney = GetMoney();
	GainMoney(imoney);


	((gplayer_dispatcher*)_runner)->item_to_money(inv_index, type, count, GetMoney() - nmoney);
	return true;
}

void 
gplayer_imp::PlayerDuelRequest(const XID & target)
{
	if(IsCombatState())
	{
		_runner->error_message(S2C::ERR_INVALID_OPERATION_IN_COMBAT);
		return;
	}
	if(((gplayer*)_parent)->IsInvisible())
	{
		_runner->error_message(S2C::ERR_OPERTION_DENYED_IN_INVISIBLE);
		return;
	}
		
	_duel.PlayerDuelRequest(this,target);
}

void 
gplayer_imp::PlayerDuelReply(const XID & target,int param)
{
	if(IsCombatState())
	{
		_runner->error_message(S2C::ERR_INVALID_OPERATION_IN_COMBAT);
		return;
	}
	_duel.PlayerDuelReply(this,target,param);
}

bool
gplayer_imp::TestCreatePet(size_t pet_id)
{
	pet_data data;
	memset(&data,0,sizeof(data));
	data.honor_point = 100;
	data.pet_tid = pet_id;
	data.pet_class = 0;
	data.level = 10;
	data.color = 0;

	int index = _petman.AddPetData(data);
	if(index >= 0)
	{
		_runner->gain_pet(index, &data,sizeof(data));
	}

/*
	pet_data * pData = _petman.GetPetData(index);
	if(0 && !pData) return false;
	
	object_interface oi(this);
	object_interface::pet_param pp = {3884,4080,0.3f,0,0};
	pp.use_pet_name = 1;
	size_t name_size = _username_len;
	if(name_size > 12) name_size = 12;
	memcpy(pp.pet_name,_username,name_size);
	pp.pet_name[name_size+0] = 0x84;
	pp.pet_name[name_size+1] = 0x76;
	pp.pet_name[name_size+2] = 0x0F;
	pp.pet_name[name_size+3] = 0x5C;
	pp.pet_name[name_size+4] = 0xD7;
	pp.pet_name[name_size+5] = 0x72;
	pp.pet_name_size = name_size + 6;
	XID  who;
	bool bRst = oi.CreatePet(_parent->pos,pp, who);

//	if(bRst) _petman.ActivePet(index,who);
	return true;
	*/
	return true;
}

bool
gplayer_imp::SummonPet(size_t index)
{
	return _petman.ActivePet(this,index);
}

bool
gplayer_imp::RecallPet()
{
	return _petman.RecallPet(this);
}

void 
gplayer_imp::SendPetCommand(int cur_target,int pet_cmd, const void * buf, size_t size)
{
	_petman.PlayerPetCtrl(this, cur_target, pet_cmd, buf, size);
}

void 
gplayer_imp::Die(const XID & attacker, bool is_pariah, char attacker_mode, int taskdead)
{
	//filter��������ǰ�Ĵ���
	_filters.EF_BeforeDeath(attacker,attacker_mode);
	//����filter��ʹ��������������	
	if(_basic.hp > 0 && taskdead == 0) return;
	
	//����Ƿ��������������
	if(attacker_mode & attack_msg::PVP_DUEL)
	{
		//�����������������������⴦��
		_basic.hp = 1;
		_filters.ClearSpecFilter(filter::FILTER_MASK_DEBUFF);

		//���ÿ��ܵľ�������
		_duel.OnDeath(this, true);
		return ;
	}
	_duel.OnDeath(this, false);

	//�����Ҫ�ı�־
	_idle_mode_flag = 0;
	_seal_mode_flag = 0;

	//ȥ������ʱӦ��ȥ����filter
	_filters.ClearSpecFilter(filter::FILTER_MASK_REMOVE_ON_DEATH);

	//lgc ȥ��С���龫������Ч��,��vigor
	if(_cur_elf_info.id != (unsigned int)-1)
	{
		_cur_elf_info.vigor = 0;		
		_cur_elf_info.refresh_vigor = true;
		if(_cur_elf_info.refine_effect_active)
		{
			ElfRefineDeactivate(_cur_elf_info.refine_level);
			_runner->elf_refine_activate(0);
		}
	}		

	//�����������������������д���
	OnDeath(attacker, is_pariah, attacker_mode, taskdead);

	//
	slice * pPiece = _parent->pPiece;
	if(pPiece && pPiece->IsBorder())
	{
		extern_object_manager::SendRefreshMsg<0>(_plane,_parent,0,pPiece);
	}
	
}

bool 
gplayer_imp::CheckPlayerBindRequest()
{
	//����Ƿ��ܹ���������
	//������,ֻ����normal״̬
	//û�е�ǰ����
	if(_player_state != PLAYER_STATE_NORMAL) return false;
	if(_cur_session || HasNextSession()) return false;

	//ˮ�²��ܽ��д˲���
	if(IsUnderWater()) return false;
	
	//����״̬�²��ܷ�������
	if(((gplayer*)_parent)->IsInvisible()) return false;
	if(GetPlayerLimit(PLAYER_LIMIT_NOBIND)) return false;

	return true;
}

bool 
gplayer_imp::CheckPlayerBindInvite()
{
	//����Ƿ��ܹ���������
	//������,ֻ����normal״̬
	if(_player_state != PLAYER_STATE_NORMAL) return false;
	if(_cur_session || HasNextSession()) return false;

	//ˮ�²��ܽ��д˲���
	if(IsUnderWater()) return false;
	
	//����״̬�²��ܷ�������
	if(((gplayer*)_parent)->IsInvisible()) return false;
	if(GetPlayerLimit(PLAYER_LIMIT_NOBIND)) return false;

	return true;
	
}

bool
gplayer_imp::EnterBindMoveState(const XID & target)
{
	if(_player_state != PLAYER_STATE_NORMAL) return false;

	_player_state = PLAYER_STATE_BIND;

	gplayer *pParent = GetParent();
	pParent->bind_type = 1;
	pParent->bind_target = target.id;
	pParent->object_state |= gactive_object::STATE_IN_BIND;
	return true;
}

bool
gplayer_imp::EnterBindFollowState(const XID & target)
{
	if(_player_state != PLAYER_STATE_NORMAL) return false;
	//���������ƶ�״̬
	//���״̬���ı�player_state
	_commander->DenyCmd(controller::CMD_MOVE);
	_commander->DenyCmd(controller::CMD_PET);
	_player_state = PLAYER_STATE_BIND;

	//ȥ�����з���״̬
	_filters.RemoveFilter(FILTER_FLY_EFFECT);
	//ȥ�����г���״̬
	_petman.RecallPet(this);
	
	//��ֹ����
	_bind_to_ground ++;

	//�����ǰsession
	ClearSession();
	ClearAction();

	gplayer *pParent = GetParent();
	pParent->bind_type = 2;
	pParent->bind_target = target.id;
	pParent->object_state |= gactive_object::STATE_IN_BIND;
	return true;
}

void 
gplayer_imp::ReturnBindNormalState()
{
	if(_player_state != PLAYER_STATE_BIND)
	{
		return;
	}
	_player_state = PLAYER_STATE_NORMAL;
	gplayer *pParent = GetParent();
	if(pParent->bind_type == 2)
	{
		_commander->AllowCmd(controller::CMD_MOVE);
		_commander->AllowCmd(controller::CMD_PET);
		_bind_to_ground --;

		//����ǳ˿�
		//�����������Ҫͬ��һ������λ��
		int seq = _commander->GetCurMoveSeq();
		seq = (seq + 100) & 0xFFFF;
		_commander->SetNextMoveSeq(seq);
		_runner->trace_cur_pos(seq);
	}

	pParent->bind_type = 0;
	pParent->bind_target = 0;
	pParent->object_state &= ~gactive_object::STATE_IN_BIND;
}

void 
gplayer_imp::PlayerBindRequest(const XID & target)
{
	_bind_player.PlayerLinkRequest(this,target);
}

void 
gplayer_imp::PlayerBindInvite(const XID & target)
{
	_bind_player.PlayerLinkInvite(this,target);
}

void 
gplayer_imp::PlayerBindRequestReply(const XID & target, int param)
{
	_bind_player.PlayerLinkReqReply(this,target,param);
}

void 
gplayer_imp::PlayerBindInviteReply(const XID & target, int param)
{
	_bind_player.PlayerLinkInvReply(this,target,param);
}

void 
gplayer_imp::PlayerBindCancel()
{
	if(_player_state != PLAYER_STATE_BIND)
	{
		//��Ӧ���ǲ����ܵ�
		ASSERT(false);
		return;
	}
	_bind_player.PlayerCancel(this);
}

void 
gplayer_imp::ActiveMountState(int mount_id, unsigned short mount_color)
{
	gplayer * pPlayer = GetParent();
	bool is_mount = pPlayer->IsMountMode();
	if(is_mount) return;
	//������������
	_commander->DenyCmd(controller::CMD_ATTACK);
	_commander->DenyCmd(controller::CMD_MARKET);
	
	//��ֹ����
	_bind_to_ground ++;

	//����״̬�л� 
	pPlayer->mount_color = mount_color;
	pPlayer->mount_id  = mount_id;
	pPlayer->object_state |= gactive_object::STATE_MOUNT;

	//��������
	_runner->player_mounting(mount_id, mount_color);
}

void 
gplayer_imp::DeactiveMountState()
{
	gplayer * pPlayer = GetParent();
	bool is_mount = pPlayer->IsMountMode();
	if(!is_mount) return;

	//�ó���������ٻس���
	_petman.RecallPet(this);

	_bind_to_ground --;
	_commander->AllowCmd(controller::CMD_ATTACK);
	_commander->AllowCmd(controller::CMD_MARKET);

	//����״̬�л� 
	pPlayer->object_state &= ~gactive_object::STATE_MOUNT;
	
	//��������
	_runner->player_mounting(0,0);
}

void 
gplayer_imp::TestUnderWater()
{
	float y = path_finding::GetWaterHeight(_plane, _parent->pos.x,_parent->pos.z);
/*	if(y - WATER_BREATH_MARK <  _parent->pos.y)
	{
		_breath.ChangeState(this,false);
		_breath.SetUnderWater(y > _parent->pos.y);
	}
	else
	{
		_breath.ChangeState(this,true);
		_breath.SetUnderWater(true);
	}*/

//������Զ��������ˮ��
	float off = y - _parent->pos.y;
	if(off > 0.5f)
	{
		_breath.SetUnderWater(true,off);
		_bind_player.OnUnderWater(this,off);
		_petman.OnUnderWater(this,off);
	}
	else
	{
		_breath.SetUnderWater(false,0.0f);
	}

}

void
gplayer_imp::UpdatePlayerLayer()
{
	if(IsUnderWater())
		_layer_ctrl.UpdateLayer(LAYER_WATER);
	else if(GetParent()->IsFlyMode())
		_layer_ctrl.UpdateLayer(LAYER_AIR);
	else
		_layer_ctrl.UpdateLayer(LAYER_GROUND);
}

int 
gplayer_imp::ConvertPetDataToEggData(void * data, size_t size, pet_data * pData, const void * src_temp )
{
	//���ȼ������ݵĴ�С�Ƿ��㹻
	size_t nsize = sizeof(pe_essence);
	size_t skill_count = 0;
	for(size_t i = 0; i < pet_data::MAX_PET_SKILL_COUNT; i ++)
	{
		if(pData->skills[i].skill)
		{
			nsize += sizeof(int)*2;
			skill_count++;
		}
		else
		{
			break;
		}
	}
	if(pData->pet_class == pet_data::PET_CLASS_EVOLUTION)
	{
		nsize += sizeof(pData->evo_prop);
	}
	if(nsize > size) return -1;

	//��С�㹻���������ݵ���֯
	pe_essence * pess = (pe_essence*)data;
	pe_essence * sess = (pe_essence*)src_temp;
	
	//����Ҫ��ģ��������ݸ��ƹ���һ�ݣ�Ȼ���ٸ���pet_data���л�ԭ
	*pess = *sess;
	pess->honor_point 	= 0;
	pess->pet_tid 		= pData->pet_tid;
	pess->pet_vis_tid 	= pData->pet_vis_tid;
	pess->level		= pData->level;
	pess->color		= pData->color;
	pess->exp		= pData->exp;
	pess->skill_point	= pData->skill_point;
	pess->name_len 		= pData->name_len;
	if(pess->name_len)
	{
		memcpy(pess->name,pData->name,sizeof(pess->name));
	}
	pess->skill_count = skill_count;
	for(size_t i = 0; i < skill_count ; i ++)
	{
		pess->skills[i].skill = pData->skills[i].skill;
		pess->skills[i].level = pData->skills[i].level;
	}
	if(pData->pet_class == pet_data::PET_CLASS_EVOLUTION)
	{
		memcpy((char*)data+sizeof(*pess)+sizeof(int)*2*skill_count,&(pData->evo_prop),sizeof(pData->evo_prop));
	}

	//ת����� ����ת����Ĵ�С
	return nsize;
}

bool 
gplayer_imp::AddPetToSlot(void * arg, int inv_index)
{
	item & it = _inventory[inv_index];
	ASSERT(it.type != -1);
	pe_essence * ess = (pe_essence*)arg;
	pet_data data;
	memset(&data,0,sizeof(data));
	data.honor_point = ess->honor_point;
	data.hunger_gauge = pet_data::HUNGER_LEVEL_1;		//���ﵰ��ԭ������������״̬�ĳ���
	data.pet_tid = ess->pet_tid;
	data.pet_vis_tid = ess->pet_vis_tid;
	data.pet_egg_tid = ess->pet_egg_tid;
	data.pet_class = ess->pet_class;
	data.hp_factor = 1.0f;
	data.level = ess->level;
	data.color = ess->color;
	data.exp = ess->exp;
	data.skill_point = ess->skill_point;
	if(it.proc_type & item::ITEM_PROC_TYPE_BIND) data.is_bind |= 0x01; 
	if(it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE) data.is_bind |= 0x02;
	data.name_len = ess->name_len;
	if(data.name_len)
	{
		memcpy(data.name,ess->name,sizeof(data.name));
	}
	size_t skill_count = ess->skill_count;
	for(size_t i = 0; i < skill_count && i < pet_data::MAX_PET_SKILL_COUNT; i ++)
	{
		data.skills[i].skill = ess->skills[i].skill;
		data.skills[i].level = ess->skills[i].level;
	}

	if(ess->pet_class == pet_data::PET_CLASS_EVOLUTION)
	{
		memcpy((char *)&(data.evo_prop),(char*)ess+sizeof(*ess)+sizeof(int)*2*skill_count,sizeof(data.evo_prop));
	}
	if(data.pet_class == pet_data::PET_CLASS_SUMMON 
			|| data.pet_class == pet_data::PET_CLASS_PLANT) return false;	//�ٻ����ֲ�ﲻ�������ٻ���
	int index = _petman.AddPetData(data);
	if(index >= 0)
	{
		_runner->gain_pet(index, &data,sizeof(data));
		return true;
	}
	else
	{
		return false;
	}
}

bool
gplayer_imp::BanishPet(size_t index)
{
	return _petman.BanishPet(this,index);
}

int
gplayer_imp::PlayerSummonPet(size_t index)
{
	//if(_petman.IsPetActive()) return S2C::ERR_PET_IS_ALEARY_ACTIVE;
	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;

	//����У��ͨ��������һ��session 
	session_summon_pet * pSession = new session_summon_pet(this);
	int tid = pData->pet_vis_tid;
	if(!tid) tid = pData->pet_tid;
	pSession->SetTarget(tid,index);
	pSession->SetDelay(60);
	AddSession(pSession);
	StartSession();
	return 0;
}

int
gplayer_imp::PlayerRecallPet()
{
	if(!_petman.IsPetActive()) return S2C::ERR_PET_IS_NOT_ACTIVE;
	size_t index = _petman.GetCurActivePet();
	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;


	//����У��ͨ��������һ��session 
	session_recall_pet * pSession = new session_recall_pet(this);
	int tid = pData->pet_vis_tid;
	if(!tid) tid = pData->pet_tid;
	pSession->SetTarget(tid,index);
	pSession->SetDelay(10);
	AddSession(pSession);
	StartSession();
	return 0;
}

int
gplayer_imp::ServiceConvertPetToEgg(size_t slot_index)
{
	if(_inventory.GetEmptySlotCount() == 0) return S2C::ERR_INVENTORY_IS_FULL;

	//�����Ƿ����
	pet_data * pData = _petman.GetPetData(slot_index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;

	if(_petman.GetCurActivePet() == (int)slot_index) return S2C::ERR_PET_IS_ALEARY_ACTIVE;

	//����У��ͨ��������һ��session 
	session_restore_pet * pSession = new session_restore_pet(this);
	int tid = pData->pet_vis_tid;
	if(!tid) tid = pData->pet_tid;
	pSession->SetTarget(tid,slot_index);
	pSession->SetDelay(10*20);
	AddSession(pSession);
	StartSession();
	return 0;
}

int
gplayer_imp::PlayerBanishPet(size_t index)
{
	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;
	if(_petman.IsPetActive() && _petman.GetCurActivePet() == (int)index) return S2C::ERR_PET_IS_ALEARY_ACTIVE;

	//����У��ͨ��������һ��session 
	session_free_pet * pSession = new session_free_pet(this);
	int tid = pData->pet_vis_tid;
	if(!tid) tid = pData->pet_tid;
	pSession->SetTarget(tid,index);
	pSession->SetDelay(200);
	AddSession(pSession);
	StartSession();
	return 0;
}

void 
gplayer_imp::SetPetSlotCapacity(size_t new_size, bool notify)
{
	_petman.SetAvailPetSlot(new_size);
	if(notify) _runner->pet_room_capacity(_petman.GetAvailPetSlot());
}

size_t
gplayer_imp::GetPetSlotCapacity()
{
	return _petman.GetAvailPetSlot();
}

pet_data * 
gplayer_imp::GetPetData(size_t index)
{
	return _petman.GetPetData(index);
}

bool 
gplayer_imp::FeedPet(int food_mask, int honor)
{
	return _petman.FeedCurPet(this, food_mask ,honor);
}

int
gplayer_imp::ConvertPetToEgg(size_t slot_index)
{
	//�����ﻹԭ�ɳ��ﵰ�Ĳ���
	//���л������
	//��������Ҫ�п�λ
	if(_inventory.GetEmptySlotCount() == 0) return S2C::ERR_INVENTORY_IS_FULL;

	//�����Ƿ����
	if(slot_index >= pet_manager::MAX_PET_CAPACITY) return S2C::ERR_PET_CAN_NOT_BE_RESTORED;
	pet_data * pData = _petman.GetPetData(slot_index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;

	//�Ѿ��ų��ĳ��ﲻ�ܻ�ԭ
	if(_petman.GetCurActivePet() == (int)slot_index) return S2C::ERR_PET_IS_ALEARY_ACTIVE;

	//ȡ��ģ������ ����֤֮
	DATA_TYPE dt;
	PET_EGG_ESSENCE * ess;
	ess = (PET_EGG_ESSENCE*) world_manager::GetDataMan().get_data_ptr(pData->pet_egg_tid, ID_SPACE_ESSENCE, dt);
	if(!ess || dt != DT_PET_EGG_ESSENCE) return S2C::ERR_PET_CAN_NOT_BE_RESTORED;

	//����Ǯ�Ƿ��㹻
	if(GetMoney() < (size_t)ess->money_restored) return S2C::ERR_OUT_OF_FUND;

	//������֤ͨ��

	//�ֶ����ɺ��ʵ����ԣ��ֶ������Դ��۳�ģ������
	const item_data * idata=(const item_data*)world_manager::GetDataMan().get_item_for_sell(pData->pet_egg_tid);
	if(!idata) return S2C::ERR_SERVICE_UNAVILABLE;

	item_data data = *idata;
	char content[sizeof(pe_essence) + sizeof(int)*22];

	//����ת������
	int rst = ConvertPetDataToEggData(content, sizeof(content), pData, idata->item_content);
	if(rst <= 0) return S2C::ERR_SERVICE_UNAVILABLE;

	data.item_content = content;
	data.content_length = rst;
	data.count = 1;
	if(data.proc_type & item::ITEM_PROC_TYPE_BIND2)		//�����item_dataman ��������proc_type����װ����󶨣��������̰�
	{
		//��������������а�
		data.proc_type |= item::ITEM_PROC_TYPE_NODROP |
			item::ITEM_PROC_TYPE_NOTHROW|
			item::ITEM_PROC_TYPE_NOSELL |
			item::ITEM_PROC_TYPE_NOTRADE|
			item::ITEM_PROC_TYPE_BIND;

		data.proc_type &= ~(item::ITEM_PROC_TYPE_BIND2);

		UpdateMallConsumptionBinding(data.type, data.proc_type, data.count);
	}
	else
	{
		if(pData->is_bind & 0x01)
		{
			data.proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
		}
		if(pData->is_bind & 0x02)
		{
			data.proc_type |= item::ITEM_PROC_TYPE_CAN_WEBTRADE;
		}
	}

	//����Ʒ�ָ�����������
	rst = _inventory.Push(data);
	if(rst < 0) return S2C::ERR_SERVICE_UNAVILABLE;		//���������ʧ��
	ASSERT(data.count == 0);
	_runner->obtain_item(data.type,data.expire_date,1 ,_inventory[rst].count, 0,rst);

	//���ٽ�Ǯ
	SpendMoney(ess->money_restored);
	_runner->spend_money(ess->money_restored);

	//ɾ��һ������
	_petman.FreePet(this,slot_index);

	GLog::log(GLOG_INFO,"�û�%d��ԭ�˳��ﵰ%d",_parent->ID.id,data.type);
	//���سɹ�
	return 0;
}

bool 
gplayer_imp::OI_IsMafiaMember()
{
	return GetParent()->id_mafia;
}

int
gplayer_imp::OI_GetMafiaID()
{
	return GetParent()->id_mafia;
}

char
gplayer_imp::OI_GetMafiaRank()
{
	return GetParent()->rank_mafia;
}

bool 
gplayer_imp::OI_IsMafiaMaster()
{
	return GetParent()->rank_mafia == 2;
}

bool 
gplayer_imp::OI_IsFactionAlliance(int fid)
{
	return _faction_alliance.find(fid) != _faction_alliance.end();
}

bool 
gplayer_imp::OI_IsFactionHostile(int fid)
{
	return _faction_hostile.find(fid) != _faction_hostile.end();
}

bool 
gplayer_imp::CheckGMPrivilege()
{
	return ((gplayer_controller*)_commander)->HasGMPrivilege();
}

void 
gplayer_imp::DBSetPetData(size_t index, const void * data, size_t size)
{
	_petman.DBSetPetData(index,data,size);
}

int 
gplayer_imp::CheckItemPrice(int inv_index, int item_id)
{
	if(IsItemExist(inv_index,item_id,1))
	{
		return _inventory[inv_index].price;
	}
	else
	{
		return 0;
	}
}

void 
gplayer_imp::DropSpecItem(bool isProtected, const XID & owner)
{
	bool bCanDrop = _player_state == PLAYER_STATE_NORMAL ||
			_player_state == PLAYER_DISCONNECT ||
			_player_state == PLAYER_SIT_DOWN ||
			_player_state == PLAYER_STATE_BIND;
	if(!bCanDrop) return;
	if(_free_pvp_mode) return;
	if(_basic.level <= LOW_PROTECT_LEVEL) return;
	if(_nonpenalty_pvp_state || world_manager::GetWorldFlag().nonpenalty_pvp_flag) return;
	if(_inventory.Find(0, ITEM_POPPET_DUMMY_ID2) >= 0) return;
	if(_inventory.Find(0, ITEM_POPPET_DUMMY_ID) >= 0) return;
	if(_inventory.Find(0, ITEM_POPPET_DUMMY_ID3) >= 0) return;
	if(!_pvp_enable_flag) return;
	enum 
	{
		NO_DROP_TYPE = item::ITEM_PROC_TYPE_NODROP | item::ITEM_PROC_TYPE_NOTHROW | item::ITEM_PROC_TYPE_NOTRADE |item::ITEM_PROC_TYPE_BIND
	};
	for(size_t i=0; i<_inventory.Size(); i++)
	{
		if(_inventory[i].type == -1) continue;
		item & it = _inventory[i];
		if(it.proc_type & NO_DROP_TYPE) continue;
		if(!world_manager::IsDeathDropItem(it.type)) continue;
		ThrowInvItem(i,it.count,isProtected,S2C::DROP_TYPE_DEATH,owner);
	}
}

void 
gplayer_imp::DropMoneyAmount(size_t money, bool isProtected)
{ 
	bool bCanDrop = _player_state == PLAYER_STATE_NORMAL ||
			_player_state == PLAYER_DISCONNECT ||
			_player_state == PLAYER_SIT_DOWN ||
			_player_state == PLAYER_STATE_BIND;
	if(!bCanDrop) return;
	if(_free_pvp_mode) return;
	if(_basic.level <= LOW_PROTECT_LEVEL) return;
	if(_nonpenalty_pvp_state || world_manager::GetWorldFlag().nonpenalty_pvp_flag) return;
	if(_inventory.Find(0, ITEM_POPPET_DUMMY_ID2) >= 0) return;
	if(_inventory.Find(0, ITEM_POPPET_DUMMY_ID) >= 0) return;
	if(_inventory.Find(0, ITEM_POPPET_DUMMY_ID3) >= 0) return;
	if(!_pvp_enable_flag) return;
	PlayerDropMoney(money,isProtected); 
}

size_t 
gplayer_imp::OI_GetTrashBoxCapacity(int where)
{
	if(where == IL_TRASH_BOX)
		return _trashbox.GetTrashBoxSize();
	else if(where == IL_TRASH_BOX2)
		return _trashbox.GetTrashBoxSize2();
	else if(where == IL_TRASH_BOX3)
		return _trashbox.GetTrashBoxSize3();
	else if(where == IL_TRASH_BOX4)
		return _trashbox.GetTrashBoxSize4();

	ASSERT(false);
	return 0;
}

int 
gplayer_imp::OI_GetTrashBoxDetail(int where, GDB::itemdata * list, size_t size)
{ 
	return GetTrashInventory(where).GetDBData(list,size);
}

bool 
gplayer_imp::OI_IsTrashBoxModified()
{
	return IsTrashBoxChanged();
}

bool 
gplayer_imp::OI_IsEquipmentModified()
{
	return _eq_change_counter;
}

size_t
gplayer_imp::OI_GetTrashBoxMoney()
{
	return _trashbox.GetMoney();
}

void 
gplayer_imp::EnterBattleground(int target_tag, int battle_id)
{
	if(world_manager::GetWorldTag() != 1) return;	//ֻ׼�Ӵ��������

	if(_player_state != PLAYER_STATE_NORMAL)
	{
		return;
	}

	if(target_tag == 1) return ;
	
	instance_key key;
	memset(&key,0,sizeof(key));
	GetInstanceKey(target_tag, key);
	key.target = key.essence;

	key.target.key_level4 = battle_id;

	A3DVECTOR pos(0,0,0);
	//��Player���и������� 
	if(world_manager::GetInstance()->PlaneSwitch(this ,pos,target_tag,key,0) < 0)
	{
		return ;
	}
	return ;
}

void 
gplayer_imp::EnterFactionFortress(int target_tag, int dst_factionid)
{
	if(world_manager::GetWorldTag() != 1) return;	//ֻ׼�Ӵ��������

	if(_player_state != PLAYER_STATE_NORMAL)
	{
		return;
	}

	if(target_tag == 1) return ;
	
	instance_key key;
	memset(&key,0,sizeof(key));
	GetInstanceKey(target_tag, key);
	key.target = key.essence;

	key.target.key_level3 = dst_factionid;

	A3DVECTOR pos(0,0,0);
	//��Player���и������� 
	if(world_manager::GetInstance()->PlaneSwitch(this ,pos,target_tag,key,0) < 0)
	{
		return ;
	}
	return ;
}

void 
gplayer_imp::EnterCountryBattle(int target_tag, int battle_id)
{
	if(!world_manager::GetInstance()->IsCountryTerritoryWorld()) return;		//�޶�����ӹ�ս�׶���������

	if(_player_state != PLAYER_STATE_NORMAL 
			&& _player_state != PLAYER_STATE_BIND
			&& _player_state != PLAYER_SIT_DOWN
			&& _player_state != PLAYER_STATE_MARKET)
	{
		return;
	}
	LeaveAbnormalState();

	if(target_tag == world_manager::GetWorldTag()) return ;
	
	instance_key key;
	memset(&key,0,sizeof(key));
	GetInstanceKey(target_tag, key);
	key.target = key.essence;

	key.target.key_level4 = battle_id;

	A3DVECTOR pos(0,0,0);
	//��Player���и������� 
	if(world_manager::GetInstance()->PlaneSwitch(this ,pos,target_tag,key,0) < 0)
	{
		return ;
	}
	return ;
}

int
gplayer_imp::RegroupPropPoint(int str, int agi, int vit,int eng)
{
	int t = _basic.status_point + _base_prop.vitality + _base_prop.energy + _base_prop.strength + _base_prop.agility;
	int offset = player_template::Rollback(GetPlayerClass(),_base_prop, str,agi,vit,eng);
	_basic.status_point +=  offset;
	int t2 = _basic.status_point + _base_prop.vitality + _base_prop.energy + _base_prop.strength + _base_prop.agility;
	if(t != t2)
	{
		ASSERT(false && "ϴ�����������");
	}
	if(offset == 0) return 0;

	//ʹ��װ����Ч
	RefreshEquipment();
	//����װ�������ݣ������˿���
	CalcEquipmentInfo();

	//���Լ���Ϣ
	PlayerGetProperty();
	return offset;
}


int 
gplayer_imp::OI_GetDBTimeStamp()
{
	return _db_timestamp;
}

int 
gplayer_imp::OI_InceaseDBTimeStamp()
{
	_db_timestamp ++;
	return _db_timestamp;
}

void 
gplayer_imp::LeaveAbnormalState()
{
	switch(_player_state)
	{
		case PLAYER_STATE_NORMAL:
		break;
		case PLAYER_STATE_BIND:
		_bind_player.PlayerCancel(this);
		break;
		case PLAYER_SIT_DOWN:
		StandUp();
		break;

		case PLAYER_TRADE:
			GMSV::DiscardTrade(_trade_obj->GetTradeID(),_parent->ID.id);
			//�ص��ȴ�������ɵ�״̬
			_player_state = PLAYER_WAIT_TRADE_COMPLETE;
			_trade_obj->SetTimeOut(10);
		break;

		case PLAYER_WAITING_TRADE:
		case PLAYER_WAIT_TRADE_COMPLETE:
		case PLAYER_WAIT_TRADE_READ:
		case PLAYER_WAITING_FACTION_TRADE:
		case PLAYER_WAIT_LOGOUT:
		case PLAYER_DISCONNECT:
		case PLAYER_WAIT_SWITCH:
		case PLAYER_WAIT_FACTION_TRADE_READ:
		case PLAYER_STATE_TRAVEL:	//���״̬�Ѿ�����
		//do nothing
		break;
		
		case PLAYER_STATE_MARKET:
		CancelPersonalMarket();
		break;

		case PLAYER_STATE_COSMETIC:
		//������δ����� Ŀǰ���Ժ���
		break;
	}
}


bool 
gplayer_imp::CheckWaypoint(int point_index, int & point_domain)
{
	short wp = point_index & 0xFFFF;
	if(!IsWaypointActived(wp)) return false;
	abase::vector<TRANS_TARGET_SERV> & waypoint_list = globaldata_gettranstargetsserver();
	for(size_t j = 0; j < waypoint_list.size(); j ++)
	{
		if(waypoint_list[j].id == point_index)
		{
			point_domain = waypoint_list[j].domain_id;
			return true;
		}
	}
	return false;
}

bool
gplayer_imp::ReturnWaypoint(int point)
{
	abase::vector<TRANS_TARGET_SERV> & waypoint_list = globaldata_gettranstargetsserver();
	for(size_t j = 0; j < waypoint_list.size(); j ++)
	{
		if(waypoint_list[j].id == point)
		{
			A3DVECTOR pos(waypoint_list[j].vecPos.x, waypoint_list[j].vecPos.y, waypoint_list[j].vecPos.z);
			return LongJump(pos,1);	//���͵�ֻ��Ӧ������
		}
	}
	return false;
}

int gplayer_imp::CheckUseTurretScroll()
{
	if(((gplayer*)_parent)->IsInvisible()) return S2C::ERR_OPERTION_DENYED_IN_INVISIBLE;
	return 0;
}

	
gactive_imp::attack_judge
gplayer_imp::GetPetAttackHook()
{
	return gplayer_imp::__GetPetAttackHook;
}

gactive_imp::enchant_judge 
gplayer_imp::GetPetEnchantHook()
{
	return gplayer_imp::__GetPetEnchantHook;
}

gactive_imp::attack_fill 
gplayer_imp::GetPetAttackFill()
{
	return __GetPetAttackFill;
}

gactive_imp::enchant_fill 
gplayer_imp::GetPetEnchantFill()
{
	return __GetPetEnchantFill;
}

template <typename MESSAGE,typename EFFECT_MSG>
inline static  bool PetTestHarmfulEffect(gactive_imp * __this, const MESSAGE & msg, EFFECT_MSG & emsg)
{
	if(emsg.attacker_mode & attack_msg::PVP_DUEL)
	{
		emsg.is_invader = false;
		emsg.target_faction = 0xFFFFFFFF;
		return true;
	}
	bool IsInvader = false;
	XID attacker = emsg.ainfo.attacker;
	if(attacker.IsPlayerClass())
	{
		//��ǿ�ƹ��������˺�
		if(!emsg.force_attack) return false;

		//����Է�δ��PK����Ҳ���ᱻ����
		if(!(emsg.attacker_mode & attack_msg::PVP_ENABLE))
		{
			return false;
		}

		//��PK״̬�����˺�
		if(!__this->OI_IsPVPEnable()) return false;

		//���ܶ����˺�
		if(__this->OI_IsInTeam() && __this->OI_IsMember(attacker)) return false;

		//��������˰��ɱ��������ҹ����ߺͱ������ߵİ�����ͬ���򲻻ᱻ������
		int idmafia = emsg.ainfo.mafia_id;
		if(idmafia)
		{
			if(emsg.force_attack&C2S::FORCE_ATTACK_NO_MAFIA 
					&& idmafia == __this->OI_GetMafiaID()) return false;	
			if(emsg.force_attack&C2S::FORCE_ATTACK_NO_MAFIA_ALLIANCE 
					&& __this->OI_IsFactionAlliance(idmafia)) return false;	
		}
		
		//���������������������ͬ������Ҳ��ᱻ����
		if(emsg.ainfo.force_id && emsg.ainfo.force_id == __this->OI_GetForceID()) return false;

		int invader_state = __this->OI_GetInvaderState();
		//����ܹ������ǰ������ҹ�����Ϣ�Ǳ���������ô���ᱻ������
		if(invader_state == gactive_imp::INVADER_LVL_0 && (emsg.force_attack & C2S::FORCE_ATTACK_NO_WHITE))
		{
			return false;
		}

		//����Է�����PVP״̬����ش���Ϣ����Ҵ���PVP
		if(!(emsg.attacker_mode & attack_msg::PVP_DURATION))
		{
			MSG msg;
			BuildMessage(msg,GM_MSG_ENABLE_PVP_DURATION,attacker,
					__this->_parent->ID,__this->_parent->pos,0);
			__this->_plane->PostLazyMessage(msg);
		}

		//����������߲��Ǻ�����Ϊ�Ƿ�����
		//IsInvader = (invader_state != gactive_imp::INVADER_LVL_2);
		//���ڸĳ�һ�ɷǷ�������������������Ҳ�ǷǷ�����
		IsInvader = true;
	}
	emsg.is_invader = IsInvader;
	return true;
}

bool 
gplayer_imp::__GetPetAttackHook(gactive_imp * __this, const MSG & msg, attack_msg & amsg)
{
	//���г���Ĺ����ж�������һ���һ��
	//����Ǿ��� ���������ж� ���ﲻ���д��� 
	return PetTestHarmfulEffect(__this, msg, amsg);
}

bool 
gplayer_imp::__GetPetEnchantHook(gactive_imp * __this, const MSG & msg,enchant_msg & emsg)
{
	XID attacker = emsg.ainfo.attacker;
	if(emsg.helpful)
	{
		//�Ѻü���
		XID attacker = emsg.ainfo.attacker;
		bool pvpcombat = __this->OI_IsInPVPCombatState();
		if(attacker.IsPlayerClass())
		{
			if(!(emsg.attacker_mode & attack_msg::PVP_ENABLE) && pvpcombat)
			{
				return false;
			}
		}
		int invader_state = __this->OI_GetInvaderState();
		emsg.is_invader = pvpcombat && (invader_state != INVADER_LVL_0) && (emsg.attacker_mode & attack_msg::PVP_ENABLE);
		
		//����Է�����PVP״̬�������Լ�����PVP״̬����һ����Ϣ�öԷ���ΪPVP״̬
		if(!(emsg.attacker_mode & attack_msg::PVP_DURATION) && pvpcombat)
		{
			__this->SendTo<0>(GM_MSG_ENABLE_PVP_DURATION,attacker,0);
		}
	}
	else
	{
		//�к����� ��ͬ����ͨ����
		return PetTestHarmfulEffect(__this, msg, emsg);
	}
	return true;
}

template < typename EFFECT_MSG>
inline static void FillPetAttackMsg(gactive_imp * __this, EFFECT_MSG &attack)
{
	bool is_pvpenable = __this->OI_IsPVPEnable();
	if(!is_pvpenable) attack.force_attack = 0;
	if(attack.force_attack) attack.force_attack |= C2S::FORCE_ATTACK;
	if(attack.force_attack & (C2S::FORCE_ATTACK_NO_MAFIA|C2S::FORCE_ATTACK_NO_MAFIA_ALLIANCE))
	{
		//��������˲��˰��ɳ�Ա/����ͬ����������Ӧ��־
		attack.ainfo.mafia_id = __this->OI_GetMafiaID();
	}
	if(attack.force_attack & C2S::FORCE_ATTACK_NO_SAME_FORCE)
	{
		attack.ainfo.force_id = __this->OI_GetForceID();
	}

	attack.attacker_mode = is_pvpenable?attack_msg::PVP_ENABLE:0;
	if(__this->OI_GetTaskMask() & TASK_MASK_KILL_PLAYER) attack.attacker_mode |= attack_msg::PVP_FEEDBACK_KILL;

}

void 
gplayer_imp::__GetPetAttackFill(gactive_imp * __this, attack_msg & attack)
{
	FillPetAttackMsg(__this, attack);
}

void 
gplayer_imp::__GetPetEnchantFill(gactive_imp * __this, enchant_msg & enchant)
{
	FillPetAttackMsg(__this, enchant);
}

void 
gplayer_imp::SetPetLeaderData(pet_leader_prop & data)
{
	data.is_pvp_enable = OI_IsPVPEnable();
	data.pvp_combat_timer = GetPVPCombatTimer();
	data.mafia_id = OI_GetMafiaID();
	data.team_count = GetTeamMemberNum();
	data.team_efflevel = _team.GetEffLevel();
	data.wallow_level = _team.GetWallowLevel();
	data.profit_level = _profit_level;
	if(data.team_efflevel <=0) data.team_efflevel = _basic.level;
	GetMemberList(data.teamlist);
	data.team_id = 0;
	data.team_seq = 0;
	_team.GetTeamID(data.team_id, data.team_seq);
	data.cs_index = GetCSIndex();
	data.cs_sid = GetCSSid();
	data.duel_target = _duel_target;
	data.task_mask = _task_mask;
	data.force_id = _player_force.GetForce();
	data.invader_state = _invader_state;
	data.free_pvp_mode = _free_pvp_mode;
}

bool gplayer_imp::ResurrectPet(size_t index)
{
	int rst = _petman.ResurrectPet(this,index);
	if(!rst) return true;
	_runner->error_message(rst);
	return false;
}

void 
gplayer_imp::NotifyMasterInfoToPet(bool at_once)
{
	_petman.NotifyMasterInfo(this, at_once);
	_plantpetman.NotifyMasterInfo(this);
}

void 
gplayer_imp::OnDuelStart(const XID & target)
{
	gactive_imp::OnDuelStart(target);
	_duel_target = target.id;
	NotifyMasterInfoToPet(true);
}

void 
gplayer_imp::OnDuelStop()
{
	gactive_imp::OnDuelStop();
	_duel_target = 0;
	NotifyMasterInfoToPet(true);
}

void 
gplayer_imp::SetExtRestParam(const void * buf, size_t size)
{
	if(size < sizeof(int) || !buf)
	{	
		//��С����ȷ��������
		return;
	}
	const int * content = (const int*)buf;
	int version = content[0];
	switch(version)
	{
		case 1:
		if(size != sizeof(int) + sizeof(int) * 2) return;
		_mafia_rest_time = content[1];
		_mafia_rest_counter_time = content[2];
		break;
		default:
		break;
	}
}

void 
gplayer_imp::GetExtRestParam(archive & ar)
{
	ar << (int)1; //version 1;
	ar << _mafia_rest_time << _mafia_rest_counter_time;
}

void 
gplayer_imp::SayHelloToNPC(const XID & target)
{
	SendTo<0>(GM_MSG_SERVICE_HELLO,target,GetFaction());
}

XID 
gplayer_imp::OI_GetPetID()
{
	return _petman.GetCurPet();
}

void
gplayer_imp::OI_ResurrectPet()
{
	if(int rst = _petman.ResurrectPet(this))
	{
		_runner->error_message(rst);
	}
}

void 
gplayer_imp::Notify_StartAttack(const XID & target,char force_attack)
{
	if(target != _last_attack_target)
	{
		OnAntiCheatAttack(0.02f);
		_last_attack_target = target;
	}
	_petman.NotifyStartAttack(this,target, force_attack);
	_plantpetman.NotifyStartAttack(this,target, force_attack);
}

void 
gplayer_imp::FirstAcquireItem(const item_data* itemdata)
{
    if ((itemdata == NULL) || (itemdata->item_content == NULL)) return;
    int item_id = itemdata->type;

	//�״λ�ÿ�������뿨���ռ�ͼ��
	DATA_TYPE dt;
	const POKER_ESSENCE * ess = (const POKER_ESSENCE *)world_manager::GetDataMan().get_data_ptr(item_id, ID_SPACE_ESSENCE,dt);
	if(ess && dt == DT_POKER_ESSENCE)
	{
		size_t collection_idx = (size_t)ess->show_order;
		if(collection_idx < GENERALCARD_MAX_COLLECTION)
		{
			_generalcard_collection.set(collection_idx, true);
			_runner->add_generalcard_collection(collection_idx);
		}
	}
	
	if(!world_manager::IsRareItem(item_id)) return;
	if(CheckGMPrivilege() && !player_template::GetDebugMode()) return;	//��ʽ����GM���ϡ����Ʒ������
	struct 
	{
		int item_id;
		char name[MAX_USERNAME_LENGTH];

        int proc_type;
        int expire_date;
        size_t content_length;
	}data;
	memset(&data,0,sizeof(data));

	data.item_id = item_id;
	size_t  len = _username_len;
	if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
	memcpy(data.name, _username,len);

    data.proc_type = itemdata->proc_type;
    data.expire_date = itemdata->expire_date;
    data.content_length = itemdata->content_length;

    packet_wrapper buf(sizeof(data) + itemdata->content_length);
    buf.push_back(&data, sizeof(data));

    if (itemdata->content_length > 0)
        buf.push_back(itemdata->item_content, itemdata->content_length);

	broadcast_chat_msg(100, buf.data(), buf.size(), GMSV::CHAT_CHANNEL_SYSTEM, 0, 0, 0);//��ʾ��XXX�����XXX �ĸ�ʽ���ı�
	__PRINTF("�����ƶ��㲥\n");
}

bool 
gplayer_imp::ChangePetName(size_t index,const char name[] , size_t name_len)
{
	int rst = _petman.ChangePetName(this, index, name, name_len);
	if(rst)
	{
		_runner->error_message(rst);
		return false;
	}
	return true;
}

bool 
gplayer_imp::ForgetPetSkill(int skill_id)
{
	int rst = _petman.ForgetPetSkill(this, skill_id);
	if(rst)
	{
		_runner->error_message(rst);
		return false;
	}
	return true;
}

int 
gplayer_imp::LearnPetSkill(int skill_id)
{
	int level = 0;
	int rst = _petman.LearnSkill(this,skill_id, &level);
	if(rst)
	{
		_runner->error_message(rst);
		return -1;
	}
	return level;
}

bool 
gplayer_imp::IsPetExist(size_t index, int pet_tid)
{
	pet_data * pData = _petman.GetPetData(index);
	return pData != NULL && pData->pet_tid == pet_tid;
}

int 
gplayer_imp::DyePet(size_t p_index, size_t d_index)
{
	//�������Ƿ����
	pet_data * pData = _petman.GetPetData(p_index);
	if(pData == NULL) return S2C::ERR_DYE_FAILED;
	//�����Ʒ�Ƿ����
	if(d_index >= _inventory.Size()) return S2C::ERR_DYE_FAILED;
	item & dye_item = _inventory[d_index];
	if(dye_item.type == -1) return S2C::ERR_DYE_FAILED;

	//�����Ʒ�Ƿ���� ��Ⱦɫ��
	itemdataman & dataman =  world_manager::GetDataMan();
	DATA_TYPE dt;
	PET_ESSENCE * pess = (PET_ESSENCE*)dataman.get_data_ptr(pData->pet_tid, ID_SPACE_ESSENCE,dt);
	if(pess == NULL || dt != DT_PET_ESSENCE) return S2C::ERR_DYE_FAILED;
	DYE_TICKET_ESSENCE * dess = (DYE_TICKET_ESSENCE*)dataman.get_data_ptr(dye_item.type, ID_SPACE_ESSENCE,dt);
	if(dess == NULL || dt != DT_DYE_TICKET_ESSENCE) return S2C::ERR_DYE_FAILED;

	//�������Ƿ����Ⱦɫ
	if(pess->require_dye_count <= 0) return S2C::ERR_PET_CAN_NOT_BE_DYED;
	if(_petman.GetCurActivePet() == (int)p_index) return S2C::ERR_DYE_FAILED;
	
	//���Ⱦɫ���Ƿ��㹻
	if(!_inventory.IsItemExist(dye_item.type, pess->require_dye_count)) return S2C::ERR_DYE_NOT_ENOUGH;
	
	float h,s,v;
	h = abase::Rand(dess->h_min, dess->h_max);
	s = abase::Rand(dess->s_min, dess->s_max);
	v = abase::Rand(dess->v_min, dess->v_max);
	int color = hsv2rgb(h,s,v);

	unsigned short r = ((color >> 16) >> 3) & 0x1F;
	unsigned short g = ((color >> 8) >> 3) & 0x1F;
	unsigned short b = (color >> 3) & 0x1F;
	unsigned short pet_color = ((r << 10) | (g << 5) | b) & 0x7FFF;
	pet_color |= 0x8000;	//����Ⱦɫ��ǣ�������δȾɫ���
	if(!_petman.DyePet(this, p_index, pet_color)) return S2C::ERR_PET_CAN_NOT_BE_DYED;

	//ɾ����Ʒ
	RemoveItems(dye_item.type,pess->require_dye_count, S2C::DROP_TYPE_USE, true);

	//��ȷ����
	return 0;
}

namespace {
class  clear_expire_item
{
	gplayer_imp * _imp;
	bool _notify;
	char _where;
	public:
	int _min_date;
	int _cur_t;
	int _remove_count;
	uint64_t _remove_mask;
	public:
	clear_expire_item(gplayer_imp * __this, bool notify, char where):_imp(__this),_notify(notify),_where(where)
	{
		_cur_t = g_timer.get_systime();
		_min_date = 0;
		_remove_count = 0;
		_remove_mask = 0;
	}

	void operator()(item_list * list, size_t index, item & it)
	{       
		if(_cur_t >= it.expire_date)
		{       
			item& it = (*list)[index];
			_imp->UpdateMallConsumptionDestroying(it.type, it.proc_type, it.count);

			if(_notify)
			{       
				_imp->_runner->player_drop_item(_where,index,it.type,it.count,S2C::DROP_TYPE_EXPIRE);
			}
			GLog::log(GLOG_INFO,"�û�%d����Ʒ%d(%d��)������ʧ",_imp->GetParent()->ID.id,it.type, it.count);
			list->Remove(index);
			_remove_count ++; 
			_remove_mask |= 1ULL << index;
		}
		else
		{       
			if(!_min_date)
			{       
				_min_date = it.expire_date;
			}
			else if(_min_date > it.expire_date)
			{
				_min_date = it.expire_date;
			}
		}

	}
};
}

void                    
gplayer_imp::RemoveAllExpireItems()
{               
	clear_expire_item  cei1(this, true, IL_INVENTORY);
	_inventory.ForEachExpireItems(cei1);

	clear_expire_item  cei2(this, true, IL_EQUIPMENT);
	_equipment.ForEachExpireItems(cei2);
	if(cei2._remove_count)
	{       
		RefreshEquipment();
		CalcEquipmentInfo();
		_runner->equipment_info_changed(0,cei2._remove_mask, 0, 0); //�˺���ʹ����CalcEquipmentInfo�Ľ��
		IncEquipChangeFlag();
	}                               

	clear_expire_item  cei3(this, true, IL_TRASH_BOX);
	_trashbox.GetBackpack1().ForEachExpireItems(cei3);
	
	clear_expire_item  cei4(this, true, IL_TRASH_BOX2);
	_trashbox.GetBackpack2().ForEachExpireItems(cei4);
	
	clear_expire_item  cei5(this, true, IL_TRASH_BOX3);
	_trashbox.GetBackpack3().ForEachExpireItems(cei5);

	clear_expire_item  cei6(this, true, IL_TRASH_BOX4);
	_trashbox.GetBackpack4().ForEachExpireItems(cei6);

	clear_expire_item  cei7(this, true, IL_USER_TRASH_BOX);
	_user_trashbox.GetBackpack1().ForEachExpireItems(cei7);

	_expire_item_date = 0;
	if(cei1._min_date) UpdateExpireItem(cei1._min_date);
	if(cei2._min_date) UpdateExpireItem(cei2._min_date);
	if(cei3._min_date) UpdateExpireItem(cei3._min_date);
	if(cei4._min_date) UpdateExpireItem(cei4._min_date);
	if(cei5._min_date) UpdateExpireItem(cei5._min_date);
	if(cei6._min_date) UpdateExpireItem(cei6._min_date);
	if(cei7._min_date) UpdateExpireItem(cei7._min_date);
}

namespace {
class  clear_expire_addon
{
	gplayer_imp * _imp;
	bool _notify;
	char _where;
	public:
	int _min_date;
	int _cur_t;
	int _changed_count;
	uint64_t _changed_mask;
	public:
	clear_expire_addon(gplayer_imp * __this, bool notify, char where):_imp(__this),_notify(notify),_where(where)
	{
		_cur_t = g_timer.get_systime();
		_min_date = 0;
		_changed_count = 0;
		_changed_mask = 0;
	}

	void operator()(item_list * list, size_t index, item & it)
	{       
		int addon_expire = it.GetAddonExpireDate();
		if(addon_expire <= 0) return;
		if(_cur_t >= addon_expire)
		{
			if(_where == gplayer_imp::IL_EQUIPMENT)
				it.Deactivate(item::BODY,index,_imp);
			addon_expire = it.RemoveExpireAddon(_cur_t);
			if(_where == gplayer_imp::IL_EQUIPMENT)
				it.Activate(item::BODY, *list, index, _imp);
			if(_notify)
				_imp->PlayerGetItemInfo(_where, index);
			_changed_count ++;
			_changed_mask |= 1ULL << index;
		}
		if(addon_expire > 0)
		{       
			if(!_min_date)
			{       
				_min_date = addon_expire;
			}
			else if(_min_date > addon_expire)
			{
				_min_date = addon_expire;
			}
		}
	}
};
}

void 
gplayer_imp::RemoveAllExpireAddon()
{
	clear_expire_addon  cei1(this, true, IL_INVENTORY);
	_inventory.ForEachItems(cei1);

	clear_expire_addon  cei2(this, true, IL_EQUIPMENT);
	_equipment.ForEachItems(cei2);
	if(cei2._changed_count)
	{       
		RefreshEquipment();
		CalcEquipmentInfo();
		abase::octets os;
		os.reserve(cei2._changed_count*sizeof(int));
		for(int i=item::EQUIP_VISUAL_START; i<item::EQUIP_VISUAL_END; i++)
		{
			if(cei2._changed_mask & (1ULL<<i))
			{
				int type = _equipment[i].type | _equipment[i].GetIdModify();
				os.push_back(&type, sizeof(type));
			}
		}
		_runner->equipment_info_changed(cei2._changed_mask, 0, os.begin(), os.size()); //�˺���ʹ����CalcEquipmentInfo�Ľ��
		IncEquipChangeFlag();
	}                               

	clear_expire_addon  cei3(this, true, IL_TRASH_BOX);
	_trashbox.GetBackpack1().ForEachItems(cei3);
	
	clear_expire_addon  cei4(this, true, IL_TRASH_BOX2);
	_trashbox.GetBackpack2().ForEachItems(cei4);
	
	clear_expire_addon  cei5(this, true, IL_TRASH_BOX3);
	_trashbox.GetBackpack3().ForEachItems(cei5);

	clear_expire_addon  cei6(this, true, IL_TRASH_BOX4);
	_trashbox.GetBackpack4().ForEachItems(cei6);

	clear_expire_addon  cei7(this, true, IL_USER_TRASH_BOX);
	_user_trashbox.GetBackpack1().ForEachItems(cei7);

	_min_addon_expire_date = 0;
	if(cei1._min_date) UpdateMinAddonExpireDate(cei1._min_date);
	if(cei2._min_date) UpdateMinAddonExpireDate(cei2._min_date);
	if(cei3._min_date) UpdateMinAddonExpireDate(cei3._min_date);
	if(cei4._min_date) UpdateMinAddonExpireDate(cei4._min_date);
	if(cei5._min_date) UpdateMinAddonExpireDate(cei5._min_date);
	if(cei6._min_date) UpdateMinAddonExpireDate(cei6._min_date);
	if(cei7._min_date) UpdateMinAddonExpireDate(cei7._min_date);
}

bool
gplayer_imp::PlayerDoShopping(size_t goods_count,const int * order_list, int shop_tid)
{
	//������Ҫ���Ӹ�����Թ����״̬
	if(_player_state != PLAYER_SIT_DOWN && _player_state != PLAYER_STATE_NORMAL && _player_state != PLAYER_STATE_BIND) return false;

	if(goods_count == 0)
	{
		return false;
	}
	if(goods_count > _inventory.Size() || !InventoryHasSlot(goods_count))
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}
	int gifts_count = 0;

	netgame::mall & shop = world_manager::GetPlayerMall();
	int __group_id = shop.GetGroupId();	//��ǰ�������趨��group_id,lgc
	time_t __time = time(NULL);			//
	netgame::mall_order  order(_mall_order_id);
	abase::vector<netgame::mall_invoice, abase::fast_alloc<> > invoice_list;
	invoice_list.reserve(goods_count);
	
	ASSERT(netgame::mall::MAX_ENTRY == 4);
	size_t offset = 0;
	for(size_t i = 0; i < goods_count; i ++, offset += sizeof(C2S::CMD::mall_shopping::__entry) / sizeof(int))
	{
		int id = order_list[offset];
		size_t index = order_list[offset +1];
		size_t slot = order_list[offset +2];
		if(slot >= netgame::mall::MAX_ENTRY)
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}
		netgame::mall::node_t node;
		if(!shop.QueryGoods(index,node) || node.goods_id != id)
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}

		if(!node.check_owner(shop_tid))
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}

		if(node.entry[slot].cash_need <= 0)
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}
		if(IsItemForbidShop(node.goods_id)
				|| node.gift_id > 0 && IsItemForbidShop(node.gift_id))
		{
			_runner->error_message(S2C::ERR_ITEM_FORBID_SHOP);
			return true;
		}
		//lgc	
		//�ҵ���ǰ��Ч��group
		int active_group_id = 0;
		if(node.group_active && __group_id != 0)
		{
			if(__group_id == node.entry[0].group_id || __group_id == node.entry[1].group_id || __group_id == node.entry[2].group_id || __group_id == node.entry[3].group_id)
				active_group_id = __group_id;	
		}

		if(node.sale_time_active)
		{	
			if(node.entry[slot].group_id == active_group_id && node.entry[slot]._sale_time.CheckAvailable(__time))	
			{
				//���playerѡ���slot�����õ����۷�ʽ������Ҫɨ�赱ǰ��Ч���ڣ����Ƿ񻹴��ڷ��������۷�ʽ�����
				if(node.entry[slot]._sale_time.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
				{
					for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
					{
						if(node.entry[j].cash_need <= 0)
							break;
						if(node.entry[j].group_id == active_group_id 
									&& node.entry[j]._sale_time.GetType() != netgame::mall::sale_time::TYPE_NOLIMIT
									&& node.entry[j]._sale_time.CheckAvailable(__time))
						{
							_runner->mall_item_buy_failed(index, 0);
							return false;
						}
					}
				}				
			}
			else
			{
				_runner->mall_item_buy_failed(index, 0);
				return false;
			}
		}
		else if(node.entry[slot].group_id != active_group_id)
		{
			_runner->mall_item_buy_failed(index, 0);
			return false;
		}

		if(node.gift_id > 0) gifts_count ++;  //ͳ����Ʒ��

		order.AddGoods(node.goods_id, node.goods_count,node.entry[slot].cash_need, node.entry[slot].expire_time,node.entry[slot].expire_type,node.gift_id,node.gift_count,node.gift_expire_time,node.gift_log_price);

	}
	if(GetMallCash() < order.GetPointRequire())
	{
		//no engouh mall cash 
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return true;
	}
	if(!InventoryHasSlot(goods_count + gifts_count))
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	int total_cash = GetMallCash();
	int cash_used = 0;
	//��Ǯ�㹻�� ��ʼ������Ʒ
	int cur_t = g_timer.get_systime();
	int self_id = GetParent()->ID.id;
	for(size_t i = 0; i < goods_count; i ++)
	{
		int id;
		int count;
		int point;
		int expire_time;
		int expire_type;
		int gift_id;
		int gift_count;
		int gift_expire_time;
		int gift_log_price;
		bool bRst = order.GetGoods(i, id, count,point, expire_time,expire_type,gift_id,gift_count,gift_expire_time,gift_log_price);
		if(bRst)
		{
			//������Ʒ����Ʒ��log�۸�
			int log_price1 = point;
			int log_price2 = 0;
			int item_type = -1;
			int item_proc_type = 0;
			if(gift_id > 0 && gift_log_price > 0)
			{
				log_price1 = int((float)point*point/(point+gift_log_price));
				log_price2 = point - log_price1; 
			}
			
			const item_data * pItem = (const item_data*)world_manager::GetDataMan().get_item_for_sell(id);
			if(pItem)
			{
				item_data * pItem2 = DupeItem(*pItem);
				int expire_date = 0;
				if(expire_time) 
				{
					if(expire_type == netgame::mall::EXPIRE_TYPE_TIME)
					{
						//��Ч������һ������
						expire_date = cur_t + expire_time;
					}
					else
					{
						//��Ч���ǹ涨����ʧЧ
						expire_date = expire_time;
					}
				}
				int guid1 = 0;
				int guid2 = 0;
				if(pItem2->guid.guid1 != 0)
				{
					//void get_item_guid(int id, int & g1, int &g2);
					//�����Ҫ������GUID
					get_item_guid(pItem2->type, guid1,guid2);
					pItem2->guid.guid1 = guid1;
					pItem2->guid.guid2 = guid2;
				}

				pItem2->proc_type |= item::ITEM_PROC_TYPE_MALL;
				UpdateMallConsumptionShopping(pItem2->type, pItem2->proc_type, count, log_price1);
				item_type = pItem2->type;
				item_proc_type = pItem2->proc_type;
				
				int ocount = count;
				int rst =_inventory.Push(*pItem2,count,expire_date);
				ASSERT(rst >= 0 && count == 0);
				_runner->obtain_item(id,pItem2->expire_date,ocount,_inventory[rst].count, 0,rst);

				//�������³�ʼ��һ�¿��ܵ�ʱװ
				_inventory[rst].InitFromShop();

				total_cash -= log_price1;
				cash_used += log_price1;

				//��¼��־  
				GLog::formatlog("formatlog:gshop_trade:userid=%d:db_magic_number=%d:order_id=%d:item_id=%d:expire=%d:item_count=%d:cash_need=%d:cash_left=%d:guid1=%d:guid2=%d",
						self_id,_db_user_id,_mall_order_id,id,expire_date,ocount,log_price1,total_cash,guid1,guid2);
				
				invoice_list.push_back(netgame::mall_invoice(_mall_order_id, id, ocount, point,expire_date,g_timer.get_systime(), guid1,guid2));
				world_manager::TestCashItemGenerated(id, ocount);
				FreeItem(pItem2);
			}
			else
			{
				//��¼������־
				GLog::log(GLOG_ERR,"�û�%d�ڹ���ٱ�����Ʒ%dʱ������Ʒʧ��",self_id, id);
			}

			//����Ϊ������Ʒ
			if(gift_id > 0)
			{
			const item_data * pGift = (const item_data*)world_manager::GetDataMan().get_item_for_sell(gift_id);
			if(pGift)
			{
				item_data * pGift2 = DupeItem(*pGift);
				int expire_date = 0;
				if(gift_expire_time) 
				{
					//��Ч������һ������
					expire_date = cur_t + gift_expire_time;
				}
				int guid1 = 0;
				int guid2 = 0;
				if(pGift2->guid.guid1 != 0)
				{
					//void get_item_guid(int id, int & g1, int &g2);
					//�����Ҫ������GUID
					get_item_guid(pGift2->type, guid1,guid2);
					pGift2->guid.guid1 = guid1;
					pGift2->guid.guid2 = guid2;
				}

				// ͳ�Ƶ�����ֵ������Ķ��������ģ�������������д0
				UpdateMallConsumptionShopping(item_type, item_proc_type, 0, log_price2);

				int ocount = gift_count;
				int rst =_inventory.Push(*pGift2,gift_count,expire_date);
				ASSERT(rst >= 0 && gift_count == 0);
				_runner->obtain_item(gift_id,expire_date,ocount,_inventory[rst].count, 0,rst);

				//�������³�ʼ��һ�¿��ܵ�ʱװ
                _inventory[rst].InitFromShop();
				
				total_cash -= log_price2;
				cash_used += log_price2;
				//��¼��־  
				GLog::formatlog("formatlog:gshop_trade:userid=%d:db_magic_number=%d:order_id=%d:item_id=%d:expire=%d:item_count=%d:cash_need=%d:cash_left=%d:guid1=%d:guid2=%d",
						self_id,_db_user_id,_mall_order_id,gift_id, expire_date,ocount,log_price2,total_cash,guid1,guid2);
				
				world_manager::TestCashItemGenerated(gift_id, ocount);
				FreeItem(pGift2);
			}
			else
			{
				//��¼������־
				GLog::log(GLOG_ERR,"�û�%d�ڹ���ٱ�����Ʒ%dʱ������Ʒ%dʧ��",self_id, id, gift_id);
			}
			}
		}
		else
		{
			ASSERT(false);
		}
	}

	_mall_cash_offset -= cash_used;
	_runner->player_cash(GetMallCash());

	//�������¼����������б�
	/* $$$$$$$$$$$$$$ Ŀǰ���ײ����湺���б��� ��Ϊ����Ϊ�˱�̫�� ����������ʧ��
	for(size_t i =0;i < invoice_list.size(); i ++)
	{
		_mall_invoice.push_back(invoice_list[i]);
	}
	*/

	//�����Ѽ�¼���͸�gdelivery,�������ӿ��Ը������ߵĺ������������ѻ���
	GMSV::SendRefCashUsed(_parent->ID.id, cash_used, _basic.level);

	GLog::log(GLOG_INFO,"�û�%d�ڰٱ�����%d����Ʒ������%d��ʣ��%d��",self_id,goods_count,cash_used,GetMallCash());

	_mall_order_id ++;
	//���Ǽӿ�����ٶ� 
	return true;
}

bool 
gplayer_imp::CheckItemBindCondition(size_t index,  int id, int can_webtrade)
{
	if(!IsItemExist(index, id,1)) return false;
	const item & it = _inventory[index];
	if(((it.proc_type & item::ITEM_PROC_NO_BIND_MASK) == item::ITEM_PROC_NO_BIND_MASK ) && !(it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE)) return false;	//�Ѿ��󶨹���
	if(((it.proc_type & item::ITEM_PROC_NO_BIND_MASK) == item::ITEM_PROC_NO_BIND_MASK ) && (it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE) && can_webtrade) return false;	//�Ѿ��󶨹���
	if((it.proc_type & item::ITEM_PROC_TYPE_BIND) && !(it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE)) 
		return false;	//�Ѿ��󶨹���
	if((it.proc_type & item::ITEM_PROC_TYPE_BIND) && (it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE) && can_webtrade) 
		return false;
	if(it.proc_type & item::ITEM_PROC_TYPE_BIND2) return false;	//װ����󶨲��ð�
	if( !it.CheckEquipCanBind()			//�����ϵ�װ��λ�ã����Ҳ��ǳ��ﵰ�����ܰ�
		&& !(it.body!=NULL && it.body->GetItemType()==item_body::ITEM_TYPE_PET_EGG))
		return false;
	if(it.pile_limit != 1) 	return false;				//���Զѵ��Ĳ��ܰ�
	if((it.proc_type & item::ITEM_PROC_TYPE_NOTRADE) && can_webtrade)
		return false;
	return true;
}

bool gplayer_imp::BindItem(size_t index, int id, int can_webtrade)
{
	ASSERT(IsItemExist(index, id,1));
	//ǰ���Ѿ�ȷ�ϼ����Ƿ���Խ��а���
	item & it = _inventory[index];
	it.proc_type |= item::ITEM_PROC_TYPE_NODROP | 
			item::ITEM_PROC_TYPE_NOTHROW|
			item::ITEM_PROC_TYPE_NOSELL |
			item::ITEM_PROC_TYPE_NOTRADE|
			item::ITEM_PROC_TYPE_BIND;
	if(it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE)
		it.proc_type &= ~item::ITEM_PROC_TYPE_CAN_WEBTRADE;
	if(can_webtrade) it.proc_type |= item::ITEM_PROC_TYPE_CAN_WEBTRADE;

	UpdateMallConsumptionBinding(it.type, it.proc_type, it.count);
	
	//�ٴη���һ����Ʒ����
	PlayerGetItemInfo(IL_INVENTORY,index);

	_runner->player_bind_success(index, id);
	return true;
}


bool 
gplayer_imp::CheckBindItemDestory(size_t index,  int id)
{
	if(!IsItemExist(index, id,1)) return false;
	const item & it = _inventory[index];
	if(!(it.proc_type & item::ITEM_PROC_TYPE_BIND)) return false;	//δ�󶨵���Ʒ���ܽ��д�������
	if( !it.CheckEquipCanBind()			//�����ϵ�װ��λ�ã����Ҳ��ǳ��ﵰ����������
		&& !(it.body!=NULL && it.body->GetItemType()==item_body::ITEM_TYPE_PET_EGG))
		return false;
	return true;
}

bool 
gplayer_imp::DestoryBindItem(size_t index, int id)
{
	ASSERT(IsItemExist(index, id,1));
	//ǰ���Ѿ�ȷ�ϼ����Ƿ���Խ��а���
	item  it;
	_inventory.Remove(index, it);
	GDB::itemdata data;
	item_list::ItemToData(it, data);
	
	//��֯����Ʒ�����ݰ�
	packet_wrapper h1(200);
	h1 << data.id << data.index << data.count 
	   << data.max_count << data.guid1 << data.guid2 
	   << data.mask << data.proctype << data.expire_date;

	h1 << data.size;
	h1.push_back(data.data,data.size);

	//��������ݰ���֯�µ���Ʒ
	item_data nit;
	nit.type 	= ITEM_DESTROYING_ID;
	nit.count 	= 1;
	nit.pile_limit 	= 1;
	nit.equip_mask 	= 0;
	nit.proc_type 	= item::ITEM_PROC_TYPE_NODROP | 
			  item::ITEM_PROC_TYPE_NOTHROW|
			  item::ITEM_PROC_TYPE_NOSELL |
			  item::ITEM_PROC_TYPE_NOTRADE|
			  item::ITEM_PROC_TYPE_NOPUTIN_USERTRASH;
	nit.classid 	= CLS_ITEM_DUMMY;
	nit.guid.guid1 	= 0;
	nit.guid.guid2 	= 0;
	nit.price 	= 0;
	nit.expire_date = g_timer.get_systime() + 72*3600;
	nit.content_length = h1.size();
	nit.item_content = (char *)h1.data();

	//֪ͨ�ͻ���ɾ����Ʒ
	//�����ɾ����Ʒ
	_runner->player_drop_item(IL_INVENTORY,index, it.type, it.count,S2C::DROP_TYPE_USE);
	it.Release();

	//���µ���Ʒ���뵽��Ұ���֮��
	int rst = _inventory.PushInEmpty(0, nit, 1);
	if(rst >=0)
	{
		_runner->obtain_item(nit.type,nit.expire_date,1,_inventory[rst].count, IL_INVENTORY,rst);
	}
	else
	{
		ASSERT(false);	//���ǲ�Ӧ�ó��ֵ�
	}
	return true;
} 

bool 
gplayer_imp::CheckRestoreDestoryItem(size_t index, int id)
{
	if(!IsItemExist(index, id,1)) return false;
	if(id != ITEM_DESTROYING_ID) return false;
	return true;
}

bool 
gplayer_imp::RestoreDestoryItem(size_t index, int id)
{
	ASSERT(IsItemExist(index, id,1));
	
	const item  &it = _inventory[index];
	if(it.body == 0) return false;

	const void * data;
	size_t data_len;
	it.body->GetItemData(&data, data_len);

	if(data_len < sizeof(int)*10) return false;

	GDB::itemdata idata;

	raw_wrapper ar(data,data_len);
	ar >> idata.id >> idata.index >> idata.count 
	   >> idata.max_count >> idata.guid1 >> idata.guid2 
	   >> idata.mask >> idata.proctype >> idata.expire_date;

	ar >> idata.size;
	idata.data = ar.cur_data();
	if(idata.size != ar.size() - ar.offset()) return false;
	
	item new_item;
	if(!MakeItemEntry(new_item,idata)) return false;

	//ɾ��ԭ������Ʒ
	item  old_item;
	_inventory.Remove(index, old_item);
	_runner->player_drop_item(IL_INVENTORY,index, old_item.type, old_item.count,S2C::DROP_TYPE_USE);
	old_item.Release();

	//���µ���Ʒ���뵽����֮��
	int type = new_item.type;
	int expire_date = new_item.expire_date;
	int rst = _inventory.PushInEmpty(0, new_item);
	if(rst >=0)
	{
		_runner->obtain_item(type,expire_date,1,_inventory[rst].count, IL_INVENTORY,rst);
	}
	else
	{
		new_item.Release();
		ASSERT(false);	
	}

	new_item.Clear();
	return true;
}

bool
gplayer_imp::OI_GetMallInfo(int & cash, int & cash_used, int &cash_delta,  int &order_id)
{
	cash = _mall_cash;
	cash_used = _mall_cash_used;
	cash_delta = _mall_cash_offset;
	order_id = _mall_order_id;
	return true;
}

bool
gplayer_imp::OI_IsCashModified()
{
	return NeedSaveMallInfo();
}

void 
gplayer_imp::SendTeamChat(char channel, const void * buf, size_t len, const void * aux_data, size_t dsize, int use_id)
{
	if(IsInTeam())
	{
		_team.TeamChat(channel,_chat_emote,buf,len,use_id > 0?use_id:_parent->ID.id, aux_data , dsize);
	}
}

void 
gplayer_imp::SendFarCryChat(char channel, const void * msg, size_t size, const void* data, size_t dsize)
{
	if(_player_state != PLAYER_STATE_NORMAL
		&& _player_state != PLAYER_STATE_BIND
		&& _player_state != PLAYER_SIT_DOWN
		&& _player_state != PLAYER_STATE_MARKET)
	{
		return;
	}

	if(!CheckCoolDown(COOLDOWN_INDEX_FARCRY))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return;
	}

	//��ȴ
	SetCoolDown(COOLDOWN_INDEX_FARCRY, 800);

	int item_need = 0;
	int item_idx = -1;
	if(channel == GMSV::CHAT_CHANNEL_FARCRY)
	{
		item_need = WORLD_SPEAKER_ID;
		item_idx = _inventory.Find(0, item_need);
		if(item_idx < 0)
		{       
			item_need = WORLD_SPEAKER_ID2;
			item_idx = _inventory.Find(0, item_need);
			if(item_idx < 0)
			{
				_runner->error_message(S2C::ERR_ITEM_NOT_IN_INVENTORY);
				return ;
			}
		}
	}
	else if(channel == GMSV::CHAT_CHANNEL_SUPERFARCRY)
	{
		item_need = SUPERWORLD_SPEAKER_ID;
		item_idx = _inventory.Find(0, item_need);
		if(item_idx < 0)
		{
			item_need = SUPERWORLD_SPEAKER_ID2;
			item_idx = _inventory.Find(0, item_need);
			if(item_idx < 0)
			{       
				_runner->error_message(S2C::ERR_ITEM_NOT_IN_INVENTORY);
				return ;
			}
		}
	}
	else
	{
		ASSERT(false);
		return;
	}

	item& it = _inventory[item_idx];
	UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);

	_inventory.DecAmount(item_idx,1);
	_runner->use_item(gplayer_imp::IL_INVENTORY,item_idx, item_need,1);


	broadcast_chat_msg(_parent->ID.id,msg,size,channel,channel==GMSV::CHAT_CHANNEL_FARCRY?_chat_emote:6,data,dsize); 
}

void 
gplayer_imp::SendNormalChat(char channel, const void * buf, size_t len, const void* data, size_t dsize)
{
	world *pPlane = _plane;
	if(pPlane == NULL) return ;
	gplayer * pPlayer = GetParent();
	slice * pPiece = pPlayer->pPiece;
	AutoBroadcastChatMsg(pPlane,pPiece,buf,len,channel,_chat_emote,data,dsize,pPlayer->ID.id,pPlayer->base_info.level);
}

void 
gplayer_imp::SendBattleFactionChat(char channel, const void * buf, size_t len, const void* data, size_t dsize)
{
	gplayer * pPlayer = GetParent();
	_plane->BattleFactionSay(GetFaction(), buf,len,_chat_emote,data,dsize,pPlayer->ID.id,pPlayer->base_info.level);
}

void 
gplayer_imp::SendCountryChat(char channel, const void * msg, size_t size, const void* data, size_t dsize)
{
	bool is_king = GetParent()->IsKing();
	if(!GetCountryId()) return;

	if(_player_state != PLAYER_STATE_NORMAL
		&& _player_state != PLAYER_STATE_BIND
		&& _player_state != PLAYER_SIT_DOWN
		&& _player_state != PLAYER_STATE_MARKET)
	{
		return;
	}

	if(!CheckCoolDown(COOLDOWN_INDEX_COUNTRY_CHAT))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return;
	}

	if(!is_king && GetMoney() < COUNTRY_CHAT_FEE)
	{
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return;
	}

	SetCoolDown(COOLDOWN_INDEX_COUNTRY_CHAT, COUNTRY_CHAT_COOLDOWN_TIME);

	if(!is_king)
	{
		SpendMoney(COUNTRY_CHAT_FEE);
		_runner->spend_money(COUNTRY_CHAT_FEE);
	}

	country_chat_msg(_parent->ID.id,msg,size,channel,(is_king?_chat_emote|0x80:_chat_emote),data,dsize); 
}

void 
gplayer_imp::SetChatEmote(int emote_id)
{
	_chat_emote = emote_id & 0xFF;
	GMSV::SetChatEmote(GetParent()->ID.id, _chat_emote);
}

void 
gplayer_imp::ActivePetNoFeed(bool feed)
{
	_petman.ActiveNoFeed(feed);
}

void 
gplayer_imp::SetHPAutoGen(int value, float rate)
{
	_auto_hp_value = value;
	_auto_hp_percent = rate;
}

void 
gplayer_imp::SetMPAutoGen(int value, float rate)
{
	_auto_mp_value = value;
	_auto_mp_percent = rate;
}


bool 
gplayer_imp::ProduceItem(const recipe_template & rt)
{
	//��ͨ������������ѡ��ԭ��
	//����Ǯ
	if(GetMoney() < rt.fee)
	{
		//����һ���ж���Ϣ
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return false;
	}

	if(rt.equipment_need_upgrade > 0) return false;
	
	//�ж����ɺ�����Ʒ
	int item_id = 0;
	if(abase::RandUniform() > rt.null_prob)
	{
		int item_idx = abase::RandSelect(&(rt.targets[0].prob),sizeof(rt.targets[0]), 4);
		item_id = rt.targets[item_idx].id;
	}

	//�������Ƿ��㹻
	if(!GetInventory().GetEmptySlotCount())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	recipe_template::__material mlist[32];
	ASSERT(sizeof(mlist) == sizeof(rt.material_list));
	memcpy(mlist,rt.material_list,sizeof(mlist));
	int num = rt.material_num;

	int nlist[_inventory.Size()];
	memset(nlist,0,sizeof(nlist));

	int total_count = rt.material_total_count;

	//�����״̬
	int bind_result = 0;	//0 ���� 1 ��  2 װ�����
	if(rt.bind_type == 1) bind_result = 1;
	int proc_type = 0;
	if(rt.bind_type == 0) proc_type = rt.proc_type;

	//�����Ʒ�б��Ƿ���������
	for(size_t i = 0;i < _inventory.Size(); i++)
	{
		int type = _inventory[i].type;
		if(type == -1) continue;
		for(int j= 0; j < num; j++)
		{
			if(mlist[j].item_id == type)
			{
				size_t count = _inventory[i].count;
				if(count > mlist[j].count) count = mlist[j].count;
				nlist[i] = count;
				if(!(mlist[j].count -= count))
				{
					std::swap(mlist[j],mlist[num-1]);
					num --;
				}
				total_count -= count;
				if(rt.bind_type == 2)
				{
					int proc_type = _inventory[i].proc_type;
					if( proc_type & item::ITEM_PROC_TYPE_BIND)
					{
						if(proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
							bind_result = 3;
						else
							bind_result = 1;
					}
					if(proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
						bind_result = 2;
				}
				break;
			}
		}
		if(total_count == 0) break;
	}
	ASSERT(total_count >= 0);
	if(total_count > 0)
	{
		//ԭ�ϲ���
		//��������ж�
		_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
		return false;
	}
	
	//������Ʒ����
	item_data * data = NULL;
	if(item_id)
	{
		struct 
		{
			char type;
			char size;
			char name[MAX_USERNAME_LENGTH];
		} tag;
		tag.type = element_data::IMT_PRODUCE;
		size_t  len;
		const void * name;
		name = GetPlayerName(len);
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(tag.name,name,len);
		tag.size = len;

		data = world_manager::GetDataMan().generate_item_from_player(item_id,&tag,sizeof(short) + len);
		if(!data)
		{
			//��Ʒ����ʧ��
			_runner->error_message(S2C::ERR_PRODUCE_FAILED);
			return false;
		}
		data->count = rt.count;

        char buf[128] = {0};
        std::string itembuf;

        for (size_t i = 0; i < _inventory.Size(); ++i)
        {
            if (nlist[i] > 0)
            {
                snprintf(buf, sizeof(buf), "����%d, ����%d; ", _inventory[i].type, nlist[i]);
                itembuf += buf;
            }
        }

		GLog::log(GLOG_INFO, "�û�%d������%d��%d, �䷽%d, ����%s", _parent->ID.id, rt.count, item_id, rt.recipe_id, itembuf.c_str());
	}
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����%dʱδ�ɹ� �䷽%d",_parent->ID.id,item_id,rt.recipe_id);
	}

	if(data)
	{
		//��һЩ����
		if(rt.exp || rt.sp)
		{
			msg_exp_t expdata = {rt.level,rt.exp,rt.sp};
			SendTo<0>(GM_MSG_EXPERIENCE,_parent->ID,0,&expdata,sizeof(expdata));
		}

		//��������һ��������
		if(rt.produce_skill > 0)
		{
			int skill_level = GetSkillLevel(rt.produce_skill);
			if(skill_level  < rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 2);
			}
			else if(skill_level  == rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 1);
			}
		}
	}

	//�����������Ʒ����Ϣ
	SpendMoney(rt.fee);
	_runner->spend_money(rt.fee);
	//��player����ȡ��ԭ�Ϻ�������
	for(size_t i = 0; i < _inventory.Size(); i ++)
	{
		if(nlist[i]) 
		{
			item& it = _inventory[i];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, nlist[i]);

			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,i,_inventory[i].type,nlist[i],S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(i,nlist[i]);
		}
	}
	if(data)
	{
		if(data->pile_limit > 1) 
		{
			bind_result = 0;
			proc_type = 0;
		}
		if(bind_result == 1)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
		} 
		else if(bind_result == 2)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_BIND2;
		}
		else if(bind_result == 3)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND |
				item::ITEM_PROC_TYPE_CAN_WEBTRADE;
		}
		else if(rt.bind_type == 0)
		{
			data->proc_type |= proc_type;	
		}
		int rst = _inventory.Push(*data);
		if(rst >=0)
		{
		    FirstAcquireItem(data);
			//�������������Ʒ����Ϣ
			_runner->produce_once(item_id,rt.count - data->count,_inventory[rst].count, 0,rst);
		}

		if(data->count)
		{
			//ʣ������Ʒ
			DropItemData(_plane,_parent->pos,data,_parent->ID,0,0,0);
			//�����������Ҫ�ͷ��ڴ�
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			//���������ж�
			return false;
		}
		FreeItem(data);
		__PRINTF("�ɹ�����������Ʒ%d\n",item_id);
	}
	else
	{
		//��������δ�ɹ�����Ϣ
		_runner->produce_null(rt.recipe_id);
	}
	return true;
}

bool 
gplayer_imp::ProduceItem2(const recipe_template & rt,int materials[16], int idxs[16] )
{
	//�ϳɣ�����ѡ��ԭ�ϣ�bind_type��proc_type����Ч,bind_type���ȼ���
	//����Ǯ
	if(GetMoney() < rt.fee)
	{
		//����һ���ж���Ϣ
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return false;
	}

	if(rt.bind_type == 0 && rt.proc_type == 0) return false;
	if(rt.equipment_need_upgrade > 0) return false;
	if(rt.material_total_count <= 0) return false;

	//�ж����ɺ�����Ʒ
	int item_id = 0;
	if(abase::RandUniform() > rt.null_prob)
	{
		int item_idx = abase::RandSelect(&(rt.targets[0].prob),sizeof(rt.targets[0]), 4);
		item_id = rt.targets[item_idx].id;
	}

	//�������Ƿ��㹻
	if(!GetInventory().GetEmptySlotCount())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	recipe_template::__material mlist[32];
	ASSERT(sizeof(mlist) == sizeof(rt.material_list));
	memcpy(mlist,rt.material_list,sizeof(mlist));

	//�����״̬
	int bind_result = 0;	//0 ���� 1 ��  2 װ����� 3 ��Ѱ��������
	if(rt.bind_type == 1) bind_result = 1;
	int proc_type = 0;
	if(rt.bind_type == 0) proc_type = rt.proc_type;

	//���в��ϼ��
	std::set<int> unique_idx;
	for(size_t i = 0;i < 16; i++)
	{
		if(rt.material_list[i].item_id)
		{
			if(rt.material_list[i].item_id != materials[i]) return false;
			int idx = idxs[i];
			if(!_inventory.IsItemExist(idx, materials[i], rt.material_list[i].count) 
					|| !unique_idx.insert(idx).second)
			{
				_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
				return false;
			}
			mlist[i].item_id = materials[i];
			mlist[i].count = rt.material_list[i].count;

			//�����״̬
			if(rt.bind_type == 2)
			{
				int proc_type = _inventory[idx].proc_type;
				if( proc_type & item::ITEM_PROC_TYPE_BIND)
				{
					if(proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
						bind_result = 3;
					else
						bind_result = 1;
				}
				if(proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
					bind_result = 2;
			}
		}
	}
	
	//������Ʒ����
	item_data * data = NULL;
	if(item_id)
	{
		struct 
		{
			char type;
			char size;
			char name[MAX_USERNAME_LENGTH];
		} tag;
		tag.type = element_data::IMT_PRODUCE;
		size_t  len;
		const void * name;
		name = GetPlayerName(len);
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(tag.name,name,len);
		tag.size = len;

		data = world_manager::GetDataMan().generate_item_from_player(item_id,&tag,sizeof(short) + len);
		if(!data)
		{
			//��Ʒ����ʧ��
			_runner->error_message(S2C::ERR_PRODUCE_FAILED);
			return false;
		}
		data->count = rt.count;

        char buf[128] = {0};
        std::string itembuf;

        for (size_t i = 0; i < 16; ++i)
        {
            if (mlist[i].item_id > 0)
            {
                snprintf(buf, sizeof(buf), "����%d, ����%d; ", _inventory[idxs[i]].type, mlist[i].count);
                itembuf += buf;
            }
        }

		GLog::log(GLOG_INFO, "�û�%d������%d��%d, �䷽%d, ����%s", _parent->ID.id, rt.count, item_id, rt.recipe_id, itembuf.c_str());
    }
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����%dʱδ�ɹ� �䷽%d",_parent->ID.id,item_id,rt.recipe_id);
	}

	if(data)
	{
		//��һЩ����
		if(rt.exp || rt.sp)
		{
			msg_exp_t expdata = {rt.level,rt.exp,rt.sp};
			SendTo<0>(GM_MSG_EXPERIENCE,_parent->ID,0,&expdata,sizeof(expdata));
		}

		//��������һ��������
		if(rt.produce_skill > 0)
		{
			int skill_level = GetSkillLevel(rt.produce_skill);
			if(skill_level  < rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 2);
			}
			else if(skill_level  == rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 1);
			}
		}
	}

	//�����������Ʒ����Ϣ
	SpendMoney(rt.fee);
	_runner->spend_money(rt.fee);
	//��player����ȡ��ԭ�Ϻ�������
	for(size_t i = 0; i < 16; i ++)
	{
		if(mlist[i].item_id) 
		{
			int idx = idxs[i];

			item& it = _inventory[idx];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, mlist[i].count);

			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,idx,_inventory[idx].type,mlist[i].count,S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(idx, mlist[i].count);
		}
	}
	if(data)
	{
		if(data->pile_limit > 1) 
		{
			bind_result = 0;
			proc_type = 0;
		}
		if(bind_result == 1)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
		}
		else if(bind_result == 2)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_BIND2;
		}
		else if(bind_result == 3)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND |
				item::ITEM_PROC_TYPE_CAN_WEBTRADE;
		}
		else if(rt.bind_type == 0)
		{
			data->proc_type |= proc_type;	
		}
		int rst = _inventory.Push(*data);
		if(rst >=0)
		{
		    FirstAcquireItem(data);
			//�������������Ʒ����Ϣ
			_runner->produce_once(item_id,rt.count - data->count,_inventory[rst].count, 0,rst);
		}

		if(data->count)
		{
			//ʣ������Ʒ
			DropItemData(_plane,_parent->pos,data,_parent->ID,0,0,0);
			//�����������Ҫ�ͷ��ڴ�
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			//���������ж�
			return false;
		}
		FreeItem(data);
		__PRINTF("�ɹ�����������Ʒ%d\n",item_id);
	}
	else
	{
		//��������δ�ɹ�����Ϣ
		_runner->produce_null(rt.recipe_id);
	}
	return true;
}

bool 
gplayer_imp::ProduceItem3(const recipe_template & rt,int materials[16], int idxs[16], int equip_id, int equip_inv_idx, char inherit_type)
{
	//�������������Լ̳д�����װ���ľ���������ʯ������ѡ��ԭ�ϣ�bind_type��proc_type����Ч,bind_type���ȼ���
	//����Ǯ
	if(GetMoney() < rt.fee)
	{
		//����һ���ж���Ϣ
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return false;
	}

	if(rt.equipment_need_upgrade <= 0) return false;
	if(rt.count != 1) return false;
	if(rt.material_total_count <= 0) return false;

	//�ж����ɺ�����Ʒ
	int item_id = 0;
	if(abase::RandUniform() > rt.null_prob)
	{
		int item_idx = abase::RandSelect(&(rt.targets[0].prob),sizeof(rt.targets[0]), 4);
		item_id = rt.targets[item_idx].id;
	}
	//������������������������߻���Ʒ
	if(item_id <= 0) return false;
	DATA_TYPE target_eq_dt;
	const void * target_eq_ess = world_manager::GetDataMan().get_data_ptr(item_id, ID_SPACE_ESSENCE, target_eq_dt);
	if(!target_eq_ess || target_eq_dt != DT_WEAPON_ESSENCE && target_eq_dt != DT_ARMOR_ESSENCE && target_eq_dt != DT_DECORATION_ESSENCE) return false;
	
	//�������Ƿ��㹻
	if(!GetInventory().GetEmptySlotCount())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	recipe_template::__material mlist[32];
	ASSERT(sizeof(mlist) == sizeof(rt.material_list));
	memcpy(mlist,rt.material_list,sizeof(mlist));

	//�����״̬
	int bind_result = 0;	//0 ���� 1 ��  2 װ�����
	if(rt.bind_type == 1) bind_result = 1;
	int proc_type = 0;
	if(rt.bind_type == 0) proc_type = rt.proc_type;

	//������װ�����������ƷDATA_TYPE��ͬ
	if(equip_id != rt.equipment_need_upgrade) return false;
	if(!_inventory.IsItemExist(equip_inv_idx, equip_id, 1)) return false;
	item & eq_it = _inventory[equip_inv_idx];
	if(eq_it.body == NULL) return false;
	DATA_TYPE eq_dt;
	const void * eq_ess = world_manager::GetDataMan().get_data_ptr(eq_it.type, ID_SPACE_ESSENCE, eq_dt);
	if(!eq_ess || eq_dt != target_eq_dt) return false;
	if(eq_dt == DT_WEAPON_ESSENCE)
	{
		if(((WEAPON_ESSENCE*)eq_ess)->level > ((WEAPON_ESSENCE*)target_eq_ess)->level) return false;
	}
	else if(eq_dt == DT_ARMOR_ESSENCE)
	{
		if(((ARMOR_ESSENCE*)eq_ess)->level > ((ARMOR_ESSENCE*)target_eq_ess)->level) return false;
	}
	else if(eq_dt == DT_DECORATION_ESSENCE)
	{
		if(((DECORATION_ESSENCE*)eq_ess)->level > ((DECORATION_ESSENCE*)target_eq_ess)->level) return false;
	}
	//����̳д�����װ���ĵľ���������ʯ�ĳɱ�
	int inherit_fee = 0;
	int eq_refine_level = 0;
	int eq_socket_count = 0;
	int eq_stone_type[4] = {0};
	addon_data eq_engrave_addon_list[3];
	size_t eq_engrave_addon_count = 0;
	if(inherit_type & PRODUCE_INHERIT_STONE) inherit_type |= PRODUCE_INHERIT_SOCKET;	//��̳б�ʯ����̳п���
	if(rt.inherit_fee_factor < 0.000001f) inherit_type |= (PRODUCE_INHERIT_REFINE | PRODUCE_INHERIT_SOCKET | PRODUCE_INHERIT_STONE); //û�м̳з���ʱ��ǿ�Ƽ̳о���������ʯ
	if(rt.inherit_engrave_fee_factor < 0.000001f) inherit_type |= PRODUCE_INHERIT_ENGRAVE;//û�м̳з���ʱ��ǿ�Ƽ̳��Կ�����
	DATA_TYPE inherit_cfg_dt;
	UPGRADE_PRODUCTION_CONFIG * inherit_cfg_ess = (UPGRADE_PRODUCTION_CONFIG *)world_manager::GetDataMan().get_data_ptr(694, ID_SPACE_CONFIG, inherit_cfg_dt);
	if(!inherit_cfg_ess || inherit_cfg_dt != DT_UPGRADE_PRODUCTION_CONFIG) return false;
	if(inherit_type & PRODUCE_INHERIT_REFINE)
	{
		//�����ȼ��̳гɱ�	
		int material_need;
		int refine_addon = world_manager::GetDataMan().get_item_refine_addon(eq_it.type, material_need);
		if(material_need > 0 && refine_addon > 0)
		{
			eq_refine_level = eq_it.body->GetRefineLevel(refine_addon);
			if(eq_refine_level > 0)
			{
				ASSERT(eq_refine_level <= 12);
				inherit_fee += int(inherit_cfg_ess->num_refine[eq_refine_level-1] * rt.inherit_fee_factor + 0.5f);
			}
		}
	}
	if(inherit_type & PRODUCE_INHERIT_SOCKET)
	{
		//�����̳гɱ�,����������2������4����Ʒ0
		if(eq_dt == DT_WEAPON_ESSENCE)
		{
			eq_socket_count = eq_it.body->GetSocketCount();
			if(eq_socket_count > 0)
			{
				ASSERT(eq_socket_count <= 2 && ((WEAPON_ESSENCE*)target_eq_ess)->level >= 1 && ((WEAPON_ESSENCE*)target_eq_ess)->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_weapon[eq_socket_count-1][((WEAPON_ESSENCE*)target_eq_ess)->level-1] * rt.inherit_fee_factor + 0.5f);
			}
		}
		else if(eq_dt == DT_ARMOR_ESSENCE)
		{
			eq_socket_count = eq_it.body->GetSocketCount();
			if(eq_socket_count > 0)
			{
				ASSERT(eq_socket_count <= 4 && ((ARMOR_ESSENCE*)target_eq_ess)->level >= 1 && ((ARMOR_ESSENCE*)target_eq_ess)->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_armor[eq_socket_count-1][((ARMOR_ESSENCE*)target_eq_ess)->level-1] * rt.inherit_fee_factor + 0.5f);
			}	
		}
	}
	if(inherit_type & PRODUCE_INHERIT_STONE)
	{
		//��ʯ�̳гɱ�
		if(eq_socket_count > 0)
		{
			for(int i=0; i<eq_socket_count; i++)
			{
				int stone_type = eq_it.body->GetSocketType(i);
				if(stone_type <= 0) continue;
				DATA_TYPE dt;
				STONE_ESSENCE * stone_ess = (STONE_ESSENCE *) world_manager::GetDataMan().get_data_ptr(stone_type,ID_SPACE_ESSENCE,dt);
				ASSERT(dt == DT_STONE_ESSENCE && stone_ess);
				ASSERT(stone_ess->level >= 1 && stone_ess->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_stone[stone_ess->level-1] * rt.inherit_fee_factor + 0.5f);
				eq_stone_type[i] = stone_type;
			}
		}
	}
	if(inherit_type & PRODUCE_INHERIT_ENGRAVE)
	{
		//�Կ̼̳з���
		eq_engrave_addon_count = eq_it.GetEngraveAddon(&eq_engrave_addon_list[0], sizeof(eq_engrave_addon_list)/sizeof(eq_engrave_addon_list[0]));
		if(eq_engrave_addon_count > 0)
		{
			ASSERT(eq_engrave_addon_count <= 3);
			inherit_fee += int(inherit_cfg_ess->num_engrave[eq_engrave_addon_count-1] * rt.inherit_engrave_fee_factor + 0.5f);
		}	
	}
	if(inherit_fee > 0 && !CheckItemExist(ALLSPARK_ID,inherit_fee)) 
	{
		_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
		return false; 
	}
	if(rt.bind_type == 2)
	{
		if(eq_it.proc_type & item::ITEM_PROC_TYPE_BIND)
		{
			if(eq_it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
				bind_result = 3;
			else
				bind_result = 1;
		}
		if(eq_it.proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
			bind_result = 2;
	}

	//���в��ϼ��
	std::set<int> unique_idx;
	for(size_t i = 0;i < 16; i++)
	{
		if(rt.material_list[i].item_id)
		{
			if(rt.material_list[i].item_id != materials[i]) return false;
			int idx = idxs[i];
			if(!_inventory.IsItemExist(idx, materials[i], rt.material_list[i].count)
					|| !unique_idx.insert(idx).second)
			{
				_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
				return false;
			}
			mlist[i].item_id = materials[i];
			mlist[i].count = rt.material_list[i].count;

			//�����״̬
			if(rt.bind_type == 2)
			{
				int proc_type = _inventory[idx].proc_type;
				if( proc_type & item::ITEM_PROC_TYPE_BIND)
				{
					if(proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
						bind_result = 3;
					else
						bind_result = 1;
				}
				if(proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
					bind_result = 2;
			}
		}
	}
	
	//������Ʒ����
	item_data * data = NULL;
	if(item_id)
	{
		struct 
		{
			char type;
			char size;
			char name[MAX_USERNAME_LENGTH];
		} tag;
		tag.type = element_data::IMT_PRODUCE;
		size_t  len;
		const void * name;
		name = GetPlayerName(len);
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(tag.name,name,len);
		tag.size = len;

		data = world_manager::GetDataMan().generate_item_from_player(item_id,&tag,sizeof(short) + len);
		if(!data)
		{
			//��Ʒ����ʧ��
			_runner->error_message(S2C::ERR_PRODUCE_FAILED);
			return false;
		}
		data->count = rt.count;

        char buf[128] = {0};
        std::string itembuf;

        for (size_t i = 0; i < 16; ++i)
        {
            if (mlist[i].item_id > 0)
            {
                snprintf(buf, sizeof(buf), "����%d, ����%d; ", _inventory[idxs[i]].type, mlist[i].count);
                itembuf += buf;
            }
        }

        snprintf(buf, sizeof(buf), "����%d, ����%d.", equip_id, 1); 
        itembuf += buf;

        GLog::log(GLOG_INFO, "�û�%d������%d��%d, �䷽%d, ����%s", _parent->ID.id, rt.count, item_id, rt.recipe_id, itembuf.c_str());
    }
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����%dʱδ�ɹ� �䷽%d",_parent->ID.id,item_id,rt.recipe_id);
	}

	if(data)
	{
		//��һЩ����
		if(rt.exp || rt.sp)
		{
			msg_exp_t expdata = {rt.level,rt.exp,rt.sp};
			SendTo<0>(GM_MSG_EXPERIENCE,_parent->ID,0,&expdata,sizeof(expdata));
		}

		//��������һ��������
		if(rt.produce_skill > 0)
		{
			int skill_level = GetSkillLevel(rt.produce_skill);
			if(skill_level  < rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 2);
			}
			else if(skill_level  == rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 1);
			}
		}
	}

	//�����������Ʒ����Ϣ
	SpendMoney(rt.fee);
	_runner->spend_money(rt.fee);
	//ɾ��������װ���Լ��̳з���

	item& it = _inventory[equip_inv_idx];
	UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
	
	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,equip_inv_idx,equip_id,1,S2C::DROP_TYPE_PRODUCE);
	_inventory.DecAmount(equip_inv_idx, 1);
	if(inherit_fee > 0)
		RemoveItems(ALLSPARK_ID, inherit_fee, S2C::DROP_TYPE_USE, true);
	//��player����ȡ��ԭ�Ϻ�������
	for(size_t i = 0; i < 16; i ++)
	{
		if(mlist[i].item_id) 
		{
			int idx = idxs[i];

			item& it = _inventory[idx];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, mlist[idx].count);

			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,idx,_inventory[idx].type,mlist[i].count,S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(idx, mlist[i].count);
		}
	}
	if(data)
	{
		if(data->pile_limit > 1) 
		{
			bind_result = 0;
			proc_type = 0;
		}
		if(bind_result == 1)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
		} 
		else if(bind_result == 2)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_BIND2;
		}
		else if(bind_result == 3)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND |
				item::ITEM_PROC_TYPE_CAN_WEBTRADE;
		}
		else if(rt.bind_type == 0)
		{
			data->proc_type |= proc_type;	
		}
		int rst = _inventory.Push(*data);
		if(rst >=0)
		{
		    FirstAcquireItem(data);

			item & target_eq_it = _inventory[rst];
			if(inherit_type & PRODUCE_INHERIT_REFINE)
			{
				//�̳д�����װ���ľ���
				if(eq_refine_level > 0)
				{
					int material_need;
					int refine_addon = world_manager::GetDataMan().get_item_refine_addon(target_eq_it.type, material_need);
					if(material_need > 0 && refine_addon > 0)
						target_eq_it.body->SetRefineLevel(refine_addon,eq_refine_level);		
				}
			}
			if(inherit_type & PRODUCE_INHERIT_SOCKET)
			{
				//�̳д�����װ���Ŀ���
				//�̳д�����װ���ı�ʯ
				if(eq_socket_count > 0)
				{
					target_eq_it.body->SetSocketAndStone(eq_socket_count,eq_stone_type);	
				}
			}
			if(inherit_type & PRODUCE_INHERIT_ENGRAVE)
			{
				//�̳д�����װ�����Կ�����
				if(eq_engrave_addon_count > 0)
				{
					target_eq_it.Engrave(&eq_engrave_addon_list[0], eq_engrave_addon_count);
				}
			}
			
			//�������������Ʒ����Ϣ
			_runner->produce_once(item_id,rt.count - data->count,_inventory[rst].count, 0,rst);
		}

		if(data->count)
		{
			//ʣ������Ʒ
			DropItemData(_plane,_parent->pos,data,_parent->ID,0,0,0);
			//�����������Ҫ�ͷ��ڴ�
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			//���������ж�
			return false;
		}
		FreeItem(data);
		__PRINTF("�ɹ�����������Ʒ%d\n",item_id);
	}
	else
	{
		//��������δ�ɹ�����Ϣ
		_runner->produce_null(rt.recipe_id);
	}
	return true;
}

bool 
gplayer_imp::ProduceItem4(const recipe_template & rt,int materials[16], int idxs[16], int equip_id, int equip_inv_idx, char & inherit_type, void **pitem, unsigned short &crc, int &eq_refine_level, int &eq_socket_count, int eq_stone_type[], addon_data eq_engrave_addon_list[3], size_t & eq_engrave_addon_count)
{
	//�¼��̳���������һ�����湦�ܣ���ҿ���ѡ���������Ի���ѡ��������
	if (*pitem)	//���pitemָ���ǲ���������
	{
		ASSERT(false);
		return false;
	}
	//����Ǯ
	if(GetMoney() < rt.fee)
	{
		//����һ���ж���Ϣ
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return false;
	}

	if(rt.equipment_need_upgrade <= 0) return false;
	if(rt.count != 1) return false;
	if(rt.material_total_count <= 0) return false;

	int item_id = 0;
	//�ж����ɺ�����Ʒ
	if(abase::RandUniform() > rt.null_prob)
	{
		int item_idx = abase::RandSelect(&(rt.targets[0].prob),sizeof(rt.targets[0]), 4);
		item_id = rt.targets[item_idx].id;
	}
	//������������������������߻���Ʒ
	if(item_id <= 0) return false;
	DATA_TYPE target_eq_dt;
	const void * target_eq_ess = world_manager::GetDataMan().get_data_ptr(item_id, ID_SPACE_ESSENCE, target_eq_dt);
	if(!target_eq_ess || target_eq_dt != DT_WEAPON_ESSENCE && target_eq_dt != DT_ARMOR_ESSENCE && target_eq_dt != DT_DECORATION_ESSENCE) return false;
	
	//�������Ƿ��㹻
	if(!GetInventory().GetEmptySlotCount())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	recipe_template::__material mlist[32];
	ASSERT(sizeof(mlist) == sizeof(rt.material_list));
	memcpy(mlist,rt.material_list,sizeof(mlist));

	//�����״̬
	int bind_result = 0;	//0 ���� 1 ��  2 װ�����
	if(rt.bind_type == 1) bind_result = 1;
	int proc_type = 0;
	if(rt.bind_type == 0) proc_type = rt.proc_type;

	//������װ�����������ƷDATA_TYPE��ͬ
	if(equip_id != rt.equipment_need_upgrade) return false;
	if(!_inventory.IsItemExist(equip_inv_idx, equip_id, 1)) return false;
	item & eq_it = _inventory[equip_inv_idx];
	if(eq_it.body == NULL) return false;
	crc = eq_it.GetCRC();
	DATA_TYPE eq_dt;
	const void * eq_ess = world_manager::GetDataMan().get_data_ptr(eq_it.type, ID_SPACE_ESSENCE, eq_dt);
	if(!eq_ess || eq_dt != target_eq_dt) return false;
	if(eq_dt == DT_WEAPON_ESSENCE)
	{
		if(((WEAPON_ESSENCE*)eq_ess)->level > ((WEAPON_ESSENCE*)target_eq_ess)->level) return false;
	}
	else if(eq_dt == DT_ARMOR_ESSENCE)
	{
		if(((ARMOR_ESSENCE*)eq_ess)->level > ((ARMOR_ESSENCE*)target_eq_ess)->level) return false;
	}
	else if(eq_dt == DT_DECORATION_ESSENCE)
	{
		if(((DECORATION_ESSENCE*)eq_ess)->level > ((DECORATION_ESSENCE*)target_eq_ess)->level) return false;
	}
	//����̳д�����װ���ĵľ���������ʯ�ĳɱ�
	int inherit_fee = 0;
	if(inherit_type & PRODUCE_INHERIT_STONE) inherit_type |= PRODUCE_INHERIT_SOCKET;	//��̳б�ʯ����̳п���
	if(rt.inherit_fee_factor < 0.000001f) inherit_type |= (PRODUCE_INHERIT_REFINE | PRODUCE_INHERIT_SOCKET | PRODUCE_INHERIT_STONE); //û�м̳з���ʱ��ǿ�Ƽ̳о���������ʯ
	if(rt.inherit_engrave_fee_factor < 0.000001f) inherit_type |= PRODUCE_INHERIT_ENGRAVE;//û�м̳з���ʱ��ǿ�Ƽ̳��Կ�����
	DATA_TYPE inherit_cfg_dt;
	UPGRADE_PRODUCTION_CONFIG * inherit_cfg_ess = (UPGRADE_PRODUCTION_CONFIG *)world_manager::GetDataMan().get_data_ptr(694, ID_SPACE_CONFIG, inherit_cfg_dt);
	if(!inherit_cfg_ess || inherit_cfg_dt != DT_UPGRADE_PRODUCTION_CONFIG) return false;
	if(inherit_type & PRODUCE_INHERIT_REFINE)
	{
		//�����ȼ��̳гɱ�	
		int material_need;
		int refine_addon = world_manager::GetDataMan().get_item_refine_addon(eq_it.type, material_need);
		if(material_need > 0 && refine_addon > 0)
		{
			eq_refine_level = eq_it.body->GetRefineLevel(refine_addon);
			if(eq_refine_level > 0)
			{
				ASSERT(eq_refine_level <= 12);
				inherit_fee += int(inherit_cfg_ess->num_refine[eq_refine_level-1] * rt.inherit_fee_factor + 0.5f);
			}
		}
	}
	if(inherit_type & PRODUCE_INHERIT_SOCKET)
	{
		//�����̳гɱ�,����������2������4����Ʒ0
		if(eq_dt == DT_WEAPON_ESSENCE)
		{
			eq_socket_count = eq_it.body->GetSocketCount();
			if(eq_socket_count > 0)
			{
				ASSERT(eq_socket_count <= 2 && ((WEAPON_ESSENCE*)target_eq_ess)->level >= 1 && ((WEAPON_ESSENCE*)target_eq_ess)->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_weapon[eq_socket_count-1][((WEAPON_ESSENCE*)target_eq_ess)->level-1] * rt.inherit_fee_factor + 0.5f);
			}
		}
		else if(eq_dt == DT_ARMOR_ESSENCE)
		{
			eq_socket_count = eq_it.body->GetSocketCount();
			if(eq_socket_count > 0)
			{
				ASSERT(eq_socket_count <= 4 && ((ARMOR_ESSENCE*)target_eq_ess)->level >= 1 && ((ARMOR_ESSENCE*)target_eq_ess)->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_armor[eq_socket_count-1][((ARMOR_ESSENCE*)target_eq_ess)->level-1] * rt.inherit_fee_factor + 0.5f);
			}	
		}
	}
	if(inherit_type & PRODUCE_INHERIT_STONE)
	{
		//��ʯ�̳гɱ�
		if(eq_socket_count > 0)
		{
			for(int i=0; i<eq_socket_count; i++)
			{
				int stone_type = eq_it.body->GetSocketType(i);
				if(stone_type <= 0) continue;
				DATA_TYPE dt;
				STONE_ESSENCE * stone_ess = (STONE_ESSENCE *) world_manager::GetDataMan().get_data_ptr(stone_type,ID_SPACE_ESSENCE,dt);
				ASSERT(dt == DT_STONE_ESSENCE && stone_ess);
				ASSERT(stone_ess->level >= 1 && stone_ess->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_stone[stone_ess->level-1] * rt.inherit_fee_factor + 0.5f);
				eq_stone_type[i] = stone_type;
			}
		}
	}
	if(inherit_type & PRODUCE_INHERIT_ENGRAVE)
	{
		//�Կ̼̳з���
		eq_engrave_addon_count = eq_it.GetEngraveAddon(&eq_engrave_addon_list[0], 3);
		if(eq_engrave_addon_count > 0)
		{
			ASSERT(eq_engrave_addon_count <= 3);
			inherit_fee += int(inherit_cfg_ess->num_engrave[eq_engrave_addon_count-1] * rt.inherit_engrave_fee_factor + 0.5f);
		}	
	}
	if(inherit_fee > 0 && !CheckItemExist(ALLSPARK_ID,inherit_fee)) 
	{
		_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
		return false; 
	}
	if(rt.bind_type == 2)
	{
		if(eq_it.proc_type & item::ITEM_PROC_TYPE_BIND)
		{
			if(eq_it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
				bind_result = 3;
			else
				bind_result = 1;
		}
		if(eq_it.proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
			bind_result = 2;
	}

	//���в��ϼ��
	std::set<int> unique_idx;
	for(size_t i = 0;i < 16; i++)
	{
		if(rt.material_list[i].item_id)
		{
			if(rt.material_list[i].item_id != materials[i]) return false;
			int idx = idxs[i];
			if(!_inventory.IsItemExist(idx, materials[i], rt.material_list[i].count)
					|| !unique_idx.insert(idx).second)
			{
				_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
				return false;
			}
			mlist[i].item_id = materials[i];
			mlist[i].count = rt.material_list[i].count;

			//�����״̬
			if(rt.bind_type == 2)
			{
				int proc_type = _inventory[idx].proc_type;
				if( proc_type & item::ITEM_PROC_TYPE_BIND)
				{
					if(proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
						bind_result = 3;
					else
						bind_result = 1;
				}
				if(proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
					bind_result = 2;
			}
		}
	}
	
	//������Ʒ����
	item_data * data = NULL;
	if(item_id)
	{
		struct 
		{
			char type;
			char size;
			char name[MAX_USERNAME_LENGTH];
		} tag;
		tag.type = element_data::IMT_PRODUCE;
		size_t  len;
		const void * name;
		name = GetPlayerName(len);
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(tag.name,name,len);
		tag.size = len;

		data = world_manager::GetDataMan().generate_item_from_player(item_id,&tag,sizeof(short) + len);
		if(!data)
		{
			//��Ʒ����ʧ��
			_runner->error_message(S2C::ERR_PRODUCE_FAILED);
			return false;
		}
		data->count = rt.count;

        char buf[128] = {0};
        std::string itembuf;

        for (size_t i = 0; i < 16; ++i)
        {
            if (mlist[i].item_id > 0)
            {
                snprintf(buf, sizeof(buf), "����%d, ����%d; ", _inventory[idxs[i]].type, mlist[i].count);
                itembuf += buf;
            }
        }

        snprintf(buf, sizeof(buf), "����%d, ����%d.", equip_id, 1);
        itembuf += buf;

        GLog::log(GLOG_INFO, "�û�%d������%d��%d, �䷽%d, ����%s", _parent->ID.id, rt.count, item_id, rt.recipe_id, itembuf.c_str());
    }
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����%dʱδ�ɹ� �䷽%d",_parent->ID.id,item_id,rt.recipe_id);
	}

	if(data)
	{
		//��һЩ����
		if(rt.exp || rt.sp)
		{
			msg_exp_t expdata = {rt.level,rt.exp,rt.sp};
			SendTo<0>(GM_MSG_EXPERIENCE,_parent->ID,0,&expdata,sizeof(expdata));
		}

		//��������һ��������
		if(rt.produce_skill > 0)
		{
			int skill_level = GetSkillLevel(rt.produce_skill);
			if(skill_level  < rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 2);
			}
			else if(skill_level  == rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 1);
			}
		}
	}

	//�����������Ʒ����Ϣ
	SpendMoney(rt.fee);
	_runner->spend_money(rt.fee);
	//ɾ���̳з���
	if(inherit_fee > 0)
		RemoveItems(ALLSPARK_ID, inherit_fee, S2C::DROP_TYPE_USE, true);
	//��player����ȡ��ԭ�Ϻ�������
	for(size_t i = 0; i < 16; i ++)
	{
		if(mlist[i].item_id) 
		{
			int idx = idxs[i];
			
			item& it = _inventory[idx];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, mlist[i].count);
				
			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,idx,_inventory[idx].type,mlist[i].count,S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(idx, mlist[i].count);
		}
	}
	if(data)
	{
		if(data->pile_limit > 1) 
		{
			bind_result = 0;
			proc_type = 0;
		}
		if(bind_result == 1)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
		} 
		else if(bind_result == 2)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_BIND2;
		}
		else if(bind_result == 3)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND |
				item::ITEM_PROC_TYPE_CAN_WEBTRADE;
		}
		else if(rt.bind_type == 0)
		{
			data->proc_type |= proc_type;	
		}
		*pitem = (void *)data;
		//������Ϣ֪ͨ�ͻ���
		_runner->produce4_item_info((int)g_timer.get_systime() + PRODUCE4_CHOOSE_ITEM_TIME, *data, 0);
		__PRINTF("�̳������ɹ�����������Ʒ%d\n",item_id);
	}
	else
	{
		//��������δ�ɹ�����Ϣ
		_runner->produce_null(rt.recipe_id);
	}
	return true;
}

bool
gplayer_imp::ProduceItem5(const recipe_template& rt,int materials[16], int idxs[16], int equip_id, int equip_inv_idx, char inherit_type)
{
    //�����������Ե��������������Լ̳д�����װ���ľ���������ʯ������ѡ��ԭ�ϣ�bind_type��proc_type����Ч,bind_type���ȼ���
	//����Ǯ
	if(GetMoney() < rt.fee)
	{
		//����һ���ж���Ϣ
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return false;
	}

	if(rt.equipment_need_upgrade <= 0) return false;
	if(rt.count != 1) return false;
	if(rt.material_total_count <= 0) return false;

	//�ж����ɺ�����Ʒ
	int item_id = 0;
	if(abase::RandUniform() > rt.null_prob)
	{
		int item_idx = abase::RandSelect(&(rt.targets[0].prob),sizeof(rt.targets[0]), 4);
		item_id = rt.targets[item_idx].id;
	}
	//������������������������߻���Ʒ
	if(item_id <= 0) return false;
    
	DATA_TYPE target_eq_dt;
	const void* target_eq_ess = world_manager::GetDataMan().get_data_ptr(item_id, ID_SPACE_ESSENCE, target_eq_dt);
	if(!target_eq_ess || target_eq_dt != DT_WEAPON_ESSENCE && target_eq_dt != DT_ARMOR_ESSENCE && target_eq_dt != DT_DECORATION_ESSENCE) return false;
	
	//�������Ƿ��㹻
	if(!GetInventory().GetEmptySlotCount())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	recipe_template::__material mlist[32];
	ASSERT(sizeof(mlist) == sizeof(rt.material_list));
	memcpy(mlist, rt.material_list, sizeof(mlist));

	//�����״̬
	int bind_result = 0;	//0 ���� 1 ��  2 װ�����
	if(rt.bind_type == 1) bind_result = 1;
	int proc_type = 0;
	if(rt.bind_type == 0) proc_type = rt.proc_type;
    
    //������װ�����������ƷDATA_TYPE��ͬ
	if(equip_id != rt.equipment_need_upgrade) return false;
	if(!_inventory.IsItemExist(equip_inv_idx, equip_id, 1)) return false;
	item& eq_it = _inventory[equip_inv_idx];
	if(eq_it.body == NULL) return false;
    
	DATA_TYPE eq_dt;
	const void* eq_ess = world_manager::GetDataMan().get_data_ptr(eq_it.type, ID_SPACE_ESSENCE, eq_dt);
	if(!eq_ess || eq_dt != target_eq_dt) return false;
    
    if(rt.inherit_addon_fee_factor < 0.000001f) inherit_type |= PRODUCE_INHERIT_ADDON; //û�м̳з��ã�ǿ�Ƽ̳и�������
	if(eq_dt == DT_WEAPON_ESSENCE)
	{
		if(((WEAPON_ESSENCE*)eq_ess)->level > ((WEAPON_ESSENCE*)target_eq_ess)->level) return false;
        if( (inherit_type & PRODUCE_INHERIT_ADDON) && ((WEAPON_ESSENCE*)target_eq_ess)->fixed_props ) return false;
	}
	else if(eq_dt == DT_ARMOR_ESSENCE)
	{
		if(((ARMOR_ESSENCE*)eq_ess)->level > ((ARMOR_ESSENCE*)target_eq_ess)->level) return false;
        if( (inherit_type & PRODUCE_INHERIT_ADDON) && ((ARMOR_ESSENCE*)target_eq_ess)->fixed_props) return false;
	}
	else if(eq_dt == DT_DECORATION_ESSENCE)
	{
		if(((DECORATION_ESSENCE*)eq_ess)->level > ((DECORATION_ESSENCE*)target_eq_ess)->level) return false;
        if( (inherit_type & PRODUCE_INHERIT_ADDON) && ((DECORATION_ESSENCE*)target_eq_ess)->fixed_props) return false;
	}
    
	//����̳д�����װ���ĵľ���������ʯ�ĳɱ�
	int inherit_fee = 0;
    int eq_refine_addon_id = 0;
    int eq_refine_material_need = 0;
	int eq_refine_level = 0;
	int eq_socket_count = 0;
	int eq_stone_type[4] = {0};
	addon_data eq_engrave_addon_list[3];
	size_t eq_engrave_addon_count = 0;
    addon_data need_inherit_addon_list[5];
    size_t need_inherit_addon_count = 0;
    
	if(inherit_type & PRODUCE_INHERIT_STONE) inherit_type |= PRODUCE_INHERIT_SOCKET;	//��̳б�ʯ����̳п���
	if(rt.inherit_fee_factor < 0.000001f) inherit_type |= (PRODUCE_INHERIT_REFINE | PRODUCE_INHERIT_SOCKET | PRODUCE_INHERIT_STONE); //û�м̳з���ʱ��ǿ�Ƽ̳о���������ʯ
	if(rt.inherit_engrave_fee_factor < 0.000001f) inherit_type |= PRODUCE_INHERIT_ENGRAVE;//û�м̳з���ʱ��ǿ�Ƽ̳��Կ�����
    
	DATA_TYPE inherit_cfg_dt;
	UPGRADE_PRODUCTION_CONFIG* inherit_cfg_ess = (UPGRADE_PRODUCTION_CONFIG*)world_manager::GetDataMan().get_data_ptr(694, ID_SPACE_CONFIG, inherit_cfg_dt);
	if(!inherit_cfg_ess || inherit_cfg_dt != DT_UPGRADE_PRODUCTION_CONFIG) return false;
    
	if(inherit_type & PRODUCE_INHERIT_REFINE)
	{
		//�����ȼ��̳гɱ�	
		eq_refine_addon_id = world_manager::GetDataMan().get_item_refine_addon(eq_it.type, eq_refine_material_need);
		if(eq_refine_material_need > 0 && eq_refine_addon_id > 0)
		{
			eq_refine_level = eq_it.body->GetRefineLevel(eq_refine_addon_id);
			if(eq_refine_level > 0)
			{
				ASSERT(eq_refine_level <= 12);
				inherit_fee += int(inherit_cfg_ess->num_refine[eq_refine_level-1] * rt.inherit_fee_factor + 0.5f);
			}
		}
	}
	if(inherit_type & PRODUCE_INHERIT_SOCKET)
	{
		//�����̳гɱ�,����������2������4����Ʒ0
		if(eq_dt == DT_WEAPON_ESSENCE)
		{
			eq_socket_count = eq_it.body->GetSocketCount();
			if(eq_socket_count > 0)
			{
				ASSERT(eq_socket_count <= 2 && ((WEAPON_ESSENCE*)target_eq_ess)->level >= 1 && ((WEAPON_ESSENCE*)target_eq_ess)->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_weapon[eq_socket_count-1][((WEAPON_ESSENCE*)target_eq_ess)->level-1] * rt.inherit_fee_factor + 0.5f);
			}
		}
		else if(eq_dt == DT_ARMOR_ESSENCE)
		{
			eq_socket_count = eq_it.body->GetSocketCount();
			if(eq_socket_count > 0)
			{
				ASSERT(eq_socket_count <= 4 && ((ARMOR_ESSENCE*)target_eq_ess)->level >= 1 && ((ARMOR_ESSENCE*)target_eq_ess)->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_armor[eq_socket_count-1][((ARMOR_ESSENCE*)target_eq_ess)->level-1] * rt.inherit_fee_factor + 0.5f);
			}	
		}
	}
	if(inherit_type & PRODUCE_INHERIT_STONE)
	{
		//��ʯ�̳гɱ�
		if(eq_socket_count > 0)
		{
			for(int i=0; i<eq_socket_count; i++)
			{
				int stone_type = eq_it.body->GetSocketType(i);
				if(stone_type <= 0) continue;
				DATA_TYPE dt;
				STONE_ESSENCE* stone_ess = (STONE_ESSENCE*) world_manager::GetDataMan().get_data_ptr(stone_type, ID_SPACE_ESSENCE, dt);
				ASSERT(dt == DT_STONE_ESSENCE && stone_ess);
				ASSERT(stone_ess->level >= 1 && stone_ess->level <= 20);
				inherit_fee += int(inherit_cfg_ess->num_stone[stone_ess->level-1] * rt.inherit_fee_factor + 0.5f);
				eq_stone_type[i] = stone_type;
			}
		}
	}
	if(inherit_type & PRODUCE_INHERIT_ENGRAVE)
	{
		//�Կ̼̳з���
		eq_engrave_addon_count = eq_it.GetEngraveAddon(&eq_engrave_addon_list[0], sizeof(eq_engrave_addon_list)/sizeof(eq_engrave_addon_list[0]));
		if(eq_engrave_addon_count > 0)
		{
			ASSERT(eq_engrave_addon_count <= 3);
			inherit_fee += int(inherit_cfg_ess->num_engrave[eq_engrave_addon_count-1] * rt.inherit_engrave_fee_factor + 0.5f);
		}	
	}
    if(inherit_type & PRODUCE_INHERIT_ADDON)
    {
        //�������Լ̳з���
        need_inherit_addon_count = eq_it.GetCanInheritAddon(&need_inherit_addon_list[0], sizeof(need_inherit_addon_list)/sizeof(need_inherit_addon_list[0]), eq_refine_addon_id);
        if(need_inherit_addon_count > 0)
        {
            ASSERT(need_inherit_addon_count <= 5);
            inherit_fee += int(inherit_cfg_ess->num_addon[need_inherit_addon_count-1] * rt.inherit_addon_fee_factor + 0.5f);
        }   
    }
    
	if(inherit_fee > 0 && !CheckItemExist(ALLSPARK_ID, inherit_fee)) 
	{
		_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
		return false; 
	}

	if(rt.bind_type == 2)
	{
		if(eq_it.proc_type & item::ITEM_PROC_TYPE_BIND)
		{
			if(eq_it.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
				bind_result = 3;
			else
				bind_result = 1;
		}
		if(eq_it.proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
			bind_result = 2;
	}

	//���в��ϼ��
	std::set<int> unique_idx;
	for(size_t i = 0;i < 16; i++)
	{
		if(rt.material_list[i].item_id)
		{
			if(rt.material_list[i].item_id != materials[i]) return false;
			int idx = idxs[i];
			if(!_inventory.IsItemExist(idx, materials[i], rt.material_list[i].count)
					|| !unique_idx.insert(idx).second)
			{
				_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
				return false;
			}
			mlist[i].item_id = materials[i];
			mlist[i].count = rt.material_list[i].count;

			//�����״̬
			if(rt.bind_type == 2)
			{
				int proc_type = _inventory[idx].proc_type;
				if( proc_type & item::ITEM_PROC_TYPE_BIND)
				{
					if(proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE && bind_result != 1)
						bind_result = 3;
					else
						bind_result = 1;
				}
				if(proc_type & item::ITEM_PROC_TYPE_BIND2 && bind_result != 1 && bind_result != 3) 
					bind_result = 2;
			}
		}
	}
	
	//������Ʒ����
	item_data* data = NULL;
	if(item_id)
	{
		struct 
		{
			char type;
			char size;
			char name[MAX_USERNAME_LENGTH];
		} tag;
		tag.type = element_data::IMT_PRODUCE;
        
		size_t len = 0;
		const void* name = GetPlayerName(len);
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(tag.name,name,len);
		tag.size = len;
        
        if(inherit_type & 0x10) //�̳и������ԣ�Ҫ������һ�������κθ������Ե���Ʒ
        {
            data = world_manager::GetDataMan().generate_item_for_shop(item_id, &tag, sizeof(short) + len);
        } 
        else 
        {
            data = world_manager::GetDataMan().generate_item_from_player(item_id, &tag,sizeof(short) + len);
        }
        
		if(!data)
		{
			//��Ʒ����ʧ��
			_runner->error_message(S2C::ERR_PRODUCE_FAILED);
			return false;
		}
		data->count = rt.count;

        char buf[128] = {0};
        std::string itembuf;

        for (size_t i = 0; i < 16; ++i)
        {
            if (mlist[i].item_id > 0)
            {
                snprintf(buf, sizeof(buf), "����%d, ����%d; ", _inventory[idxs[i]].type, mlist[i].count);
                itembuf += buf;
            }
        }

        snprintf(buf, sizeof(buf), "����%d, ����%d.", equip_id, 1); 
        itembuf += buf;

        GLog::log(GLOG_INFO, "�û�%d������%d��%d, �䷽%d, ����%s", _parent->ID.id, rt.count, item_id, rt.recipe_id, itembuf.c_str());
    }
	else
	{
		GLog::log(GLOG_INFO,"�û�%d����%dʱδ�ɹ� �䷽%d",_parent->ID.id,item_id,rt.recipe_id);
	}

	if(data)
	{
		//��һЩ����
		if(rt.exp || rt.sp)
		{
			msg_exp_t expdata = {rt.level,rt.exp,rt.sp};
			SendTo<0>(GM_MSG_EXPERIENCE,_parent->ID,0,&expdata,sizeof(expdata));
		}

		//��������һ��������
		if(rt.produce_skill > 0)
		{
			int skill_level = GetSkillLevel(rt.produce_skill);
			if(skill_level  < rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 2);
			}
			else if(skill_level  == rt.recipe_level)
			{
				IncSkillAbility(rt.produce_skill, 1);
			}
		}
	}

	//�����������Ʒ����Ϣ
	SpendMoney(rt.fee);
	_runner->spend_money(rt.fee);
    
	//ɾ��������װ���Լ��̳з���
	item& it = _inventory[equip_inv_idx];
	UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
	
	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,equip_inv_idx,equip_id,1,S2C::DROP_TYPE_PRODUCE);
	_inventory.DecAmount(equip_inv_idx, 1);
	if(inherit_fee > 0)
		RemoveItems(ALLSPARK_ID, inherit_fee, S2C::DROP_TYPE_USE, true);
	//��player����ȡ��ԭ�Ϻ�������
	for(size_t i = 0; i < 16; i ++)
	{
		if(mlist[i].item_id) 
		{
			int idx = idxs[i];

			item& it = _inventory[idx];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, mlist[idx].count);

			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,idx,_inventory[idx].type,mlist[i].count,S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(idx, mlist[i].count);
		}
	}
    
	if(data)
	{
		if(data->pile_limit > 1) 
		{
			bind_result = 0;
			proc_type = 0;
		}
		if(bind_result == 1)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;
		} 
		else if(bind_result == 2)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_BIND2;
		}
		else if(bind_result == 3)
		{
			data->proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND |
				item::ITEM_PROC_TYPE_CAN_WEBTRADE;
		}
		else if(rt.bind_type == 0)
		{
			data->proc_type |= proc_type;	
		}

		int rst = _inventory.Push(*data);
		if(rst >=0)
		{
		    FirstAcquireItem(data);
            
			item& target_eq_it = _inventory[rst];
			if(inherit_type & PRODUCE_INHERIT_REFINE)
			{
				//�̳д�����װ���ľ���
				if(eq_refine_level > 0)
				{
					int material_need;
					int refine_addon = world_manager::GetDataMan().get_item_refine_addon(target_eq_it.type, material_need);
					if(material_need > 0 && refine_addon > 0)
						target_eq_it.body->SetRefineLevel(refine_addon,eq_refine_level);		
				}
			} 
			if(inherit_type & PRODUCE_INHERIT_SOCKET)
			{
				//�̳д�����װ���Ŀ���
				//�̳д�����װ���ı�ʯ
				if(eq_socket_count > 0)
				{
					target_eq_it.body->SetSocketAndStone(eq_socket_count,eq_stone_type);	
				}
			}
			if(inherit_type & PRODUCE_INHERIT_ENGRAVE)
			{
				//�̳д�����װ�����Կ�����
				if(eq_engrave_addon_count > 0)
				{
					target_eq_it.Engrave(&eq_engrave_addon_list[0], eq_engrave_addon_count);
				}
			}
            if(inherit_type & PRODUCE_INHERIT_ADDON)
			{
                //�̳д�����װ���ĸ�������
                if(need_inherit_addon_count > 0)
                {
                    ASSERT(need_inherit_addon_count <= 5);
                    target_eq_it.InheritAddon(&need_inherit_addon_list[0], need_inherit_addon_count);
                }    
            }
            
			//�������������Ʒ����Ϣ
			_runner->produce_once(item_id,rt.count - data->count,_inventory[rst].count, 0,rst);
		}

		if(data->count)
		{
			//ʣ������Ʒ
			DropItemData(_plane,_parent->pos,data,_parent->ID,0,0,0);
			//�����������Ҫ�ͷ��ڴ�
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			//���������ж�
			return false;
		}
		FreeItem(data);
		__PRINTF("�ɹ�����������Ʒ%d\n",item_id);
	}
	else
	{
		//��������δ�ɹ�����Ϣ
		_runner->produce_null(rt.recipe_id);
	}

    return true;
}

bool
gplayer_imp::EquipSign(int ink_inv_idx, int ink_id, int equip_inv_idx, int equip_id, const char * signature, unsigned int signature_len)
{
	int ink_num = signature_len/2 > EQUIP_SIGNATURE_CLEAN_CONSUME ? signature_len/2 : EQUIP_SIGNATURE_CLEAN_CONSUME;
	//���īˮ�Ƿ����
	if(!_inventory.IsItemExist(ink_inv_idx, ink_id, ink_num)) return false; 

	//�����Ҫ����ǩ����װ���Ƿ����
	if(!_inventory.IsItemExist(equip_inv_idx, equip_id, 1)) return false;

	//���īˮ���ڱ���λ�õ���Ʒ�Ƿ�Ϊīˮ
	DATA_TYPE dt;
	DYE_TICKET_ESSENCE * ess = (DYE_TICKET_ESSENCE*) world_manager::GetDataMan().get_data_ptr(ink_id, ID_SPACE_ESSENCE, dt);
	if(!ess || dt != DT_DYE_TICKET_ESSENCE) return false;

	if (ess->usage != 1) return false;

	//���װ�����ڱ���λ�õ���Ʒ�Ƿ�Ϊ����װ����Ʒ
	DATA_TYPE target_eq_dt;
	const void * target_eq_ess = world_manager::GetDataMan().get_data_ptr(equip_id, ID_SPACE_ESSENCE, target_eq_dt);
	if(!target_eq_ess || (target_eq_dt != DT_WEAPON_ESSENCE && target_eq_dt != DT_ARMOR_ESSENCE &&
			target_eq_dt != DT_DECORATION_ESSENCE && target_eq_dt != DT_FLYSWORD_ESSENCE))
		return false;

	//����īˮ��ɫ
	float h,s,v;
	h = abase::Rand(ess->h_min, ess->h_max);
	s = abase::Rand(ess->s_min, ess->s_max);
	v = abase::Rand(ess->v_min, ess->v_max);
	int color = hsv2rgb(h,s,v);

	unsigned short r = ((color >> 16) >> 3) & 0x1F;
	unsigned short g = ((color >> 8) >> 3) & 0x1F;
	unsigned short b = (color >> 3) & 0x1F;
	
	//��ʼ�޸�װ��ǩ��
	item & it = _inventory[equip_inv_idx];
	if (!it.body)	return false;
	if (!it.body->Sign(((r << 10) | (g << 5) | b) & 0x7FFF, signature, signature_len)) return false;

	//�۳�īˮ
	item & i_it = _inventory[ink_inv_idx];    
	UpdateMallConsumptionDestroying(i_it.type, i_it.proc_type, ink_num);
		
	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,ink_inv_idx,ink_id,ink_num,S2C::DROP_TYPE_USE);
	_inventory.DecAmount(ink_inv_idx, ink_num);

	//֪ͨ�ͻ������ݸ���
	PlayerGetItemInfo(IL_INVENTORY,equip_inv_idx);
	return true;
}

bool
gplayer_imp::EngraveItem(const engrave_recipe_template& ert, size_t inv_index)
{
	if(inv_index >= _inventory.Size()) return false;
	item & it = _inventory[inv_index];
	if(it.type <= 0 || it.body == NULL) return false;
	//���װ������
	if(!(it.GetEquipMask64() & ert.equip_mask)) return false;
	int level = world_manager::GetDataMan().get_item_level(it.type);
	if(level < ert.equip_level_min || level > ert.equip_level_max) return false;
	//���ԭ��
	engrave_recipe_template::__material mlist[8];
	ASSERT(sizeof(mlist) == sizeof(ert.material_list));
	memcpy(mlist,ert.material_list,sizeof(mlist));
	int num = ert.material_num;
	int total_count = ert.material_total_count;
	
	int nlist[_inventory.Size()];
	memset(nlist,0,sizeof(nlist));
	
	for(size_t i=0; i<_inventory.Size(); i++)
	{
		int type = _inventory[i].type;
		if(type == -1) continue;
		for(int j= 0; j < num; j++)
		{
			if(mlist[j].item_id == type)
			{
				size_t count = _inventory[i].count;
				if(count > mlist[j].count) count = mlist[j].count;
				nlist[i] = count;
				if(!(mlist[j].count -= count))
				{
					std::swap(mlist[j],mlist[num-1]);
					num --;
				}
				total_count -= count;
				break;
			}
		}
		if(total_count == 0) break;
	}
	ASSERT(total_count >= 0);
	if(total_count > 0)
	{
		_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
		return false;
	}

	//��������
	addon_data addon_list[3];
	size_t addon_count;
	addon_count = abase::RandSelect(ert.prob_addon_num,4);
	for(size_t i=0; i<addon_count; i++)
	{
		int r = abase::RandSelect(&ert.target_addons[0].prob,sizeof(ert.target_addons[0]),32);
		int addon_id = ert.target_addons[r].addon_id;
		if(!world_manager::GetDataMan().generate_addon(addon_id,addon_list[i])) return false;
	}
	//�Կ�
	if(!it.Engrave(addon_list,addon_count))
	{
		return false;
	}
	PlayerGetItemInfo(IL_INVENTORY, inv_index);
	_runner->engrave_result(addon_count);
	//ȡ��ԭ��
	for(size_t i = 0; i < _inventory.Size(); i ++)
	{
		if(nlist[i]) 
		{
			item& it = _inventory[i];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, nlist[i]);

			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,i,_inventory[i].type,nlist[i],S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(i,nlist[i]);
		}
	}
	return true;
}

bool
gplayer_imp::ItemAddonRegen(const addonregen_recipe_template& arrt, size_t inv_index)
{
	if(inv_index >= _inventory.Size()) return false;
	item & it = _inventory[inv_index];
	if(it.type <= 0 || it.body == NULL) return false;
	//���װ������
	size_t i=0;
	for(; i<sizeof(arrt.equip_id_list)/sizeof(arrt.equip_id_list[0]); i++)
		if(it.type == arrt.equip_id_list[i]) break;
	if(i == sizeof(arrt.equip_id_list)/sizeof(arrt.equip_id_list[0])) return false;
	//����Ǯ
	if(GetMoney() < arrt.fee)
	{
		//����һ���ж���Ϣ
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return false;
	}
	//���ԭ��
	addonregen_recipe_template::__material mlist[8];
	ASSERT(sizeof(mlist) == sizeof(arrt.material_list));
	memcpy(mlist,arrt.material_list,sizeof(mlist));
	int num = arrt.material_num;
	int total_count = arrt.material_total_count;
	
	int nlist[_inventory.Size()];
	memset(nlist,0,sizeof(nlist));
	
	for(size_t i=0; i<_inventory.Size(); i++)
	{
		int type = _inventory[i].type;
		if(type == -1) continue;
		for(int j= 0; j < num; j++)
		{
			if(mlist[j].item_id == type)
			{
				size_t count = _inventory[i].count;
				if(count > mlist[j].count) count = mlist[j].count;
				nlist[i] = count;
				if(!(mlist[j].count -= count))
				{
					std::swap(mlist[j],mlist[num-1]);
					num --;
				}
				total_count -= count;
				break;
			}
		}
		if(total_count == 0) break;
	}
	ASSERT(total_count >= 0);
	if(total_count > 0)
	{
		_runner->error_message(S2C::ERR_NOT_ENOUGH_MATERIAL);
		return false;
	}
	//��������
	int regen_cnt;
	if( (regen_cnt = it.RegenInherentAddon()) <= 0)
	{
		_runner->addonregen_result(regen_cnt);
		return false;
	}
	PlayerGetItemInfo(IL_INVENTORY, inv_index);
	_runner->addonregen_result(regen_cnt);
	//��ȡ��ҽ�Һ�ԭ��
	if(arrt.fee)
	{
		SpendMoney(arrt.fee);
		_runner->spend_money(arrt.fee);
	}
	for(size_t i = 0; i < _inventory.Size(); i ++)
	{
		if(nlist[i]) 
		{
			item& it = _inventory[i];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, nlist[i]);

			_runner->player_drop_item(gplayer_imp::IL_INVENTORY,i,_inventory[i].type,nlist[i],S2C::DROP_TYPE_PRODUCE);
			_inventory.DecAmount(i,nlist[i]);
		}
	}
	return true;
}

void
gplayer_imp::ChangeInventorySize(int size)
{
	if(size > ITEM_LIST_MAX_SIZE) size = ITEM_LIST_MAX_SIZE;
	if(size < ITEM_LIST_BASE_SIZE) size = ITEM_LIST_BASE_SIZE;
	_inventory.SetSize(size);
	_runner->player_change_inventory_size(_inventory.Size());
}

bool 
gplayer_imp::UseItemWithArg(item_list & inv, int inv_index, int where, int item_type, const char * arg, size_t arg_size)
{
	item & it = inv[inv_index];
	bool bBroadcast = it.IsBroadcastUseMsg();
	bool bBCArg = it.IsBroadcastArgUseMsg();
	int type = it.type;
	int proc_type = it.proc_type;
	int count = 0;
	int rst = inv.UseItemWithArg(inv_index,this,count,arg, arg_size);
	if(rst >= 0)
	{
		ASSERT(rst == item_type);
		UpdateMallConsumptionDestroying(type, proc_type, count);
		_runner->use_item(where,inv_index, item_type,count,arg,arg_size);
		if(bBroadcast)
		{
			if(bBCArg)
			{
				_runner->use_item(item_type, arg, arg_size);
			}
			else
			{
				_runner->use_item(item_type);
			}
		}
		return true;
	}
	return false;
}

bool
gplayer_imp::PlayerUseItemWithArg(int where,size_t inv_index,int item_type,size_t count, const char * buf, size_t buf_size)
{
	if(where != IL_INVENTORY && where != IL_EQUIPMENT) return false;
	if(count == 0) return false;
	item_list & inv = GetInventory(where);
	if(inv_index >= inv.Size() || item_type == -1 ||
			inv[inv_index].type != item_type) return false;
	if(!inv[inv_index].CanUseWithArg(inv.GetLocation(),buf_size)) return false;
	int rst = inv[inv_index].GetUseDuration();
	if(rst < 0)
	{
		UseItemWithArg(inv,inv_index,where,item_type,buf , buf_size);
		return true;
	}
	else
	{
		//AddStartSession(new session_use_item(this,where,inv_index,item_type,count,rst));
		//$$$$$$$$$$  ������ʱû��ִ��ʱ��Ĳ���ʹ����Ʒ
		return true;
	}
}

size_t gplayer_imp::OI_GetMallOrdersCount()
{
	return _mall_invoice.size();
}

int gplayer_imp::OI_GetMallOrders(GDB::shoplog * list, size_t size)
{
	if(size < _mall_invoice.size()) return 0;
	for(size_t i = 0;i  < _mall_invoice.size(); i ++)
	{
		const netgame::mall_invoice & mi =  _mall_invoice[i];
		GDB::shoplog &sp =list[i];

		sp.order_id     = mi.order_id;
		sp.item_id      = mi.item_id;
		sp.expire       = mi.expire_date;
		sp.item_count   = mi.item_count;
		sp.order_count  = 1;
		sp.cash_need    = mi.price;
		sp.time         = mi.timestamp;
		sp.guid1        = mi.guid1;
		sp.guid2        = mi.guid2;
	}
	return (int)_mall_invoice.size();
}

int  
gplayer_imp::DyeFashionItem(size_t f_index, size_t d_index)
{
	//�����Ʒ�Ƿ����
	if(f_index >= _inventory.Size() || d_index >= _inventory.Size()) return S2C::ERR_DYE_FAILED;
	item & fashion_item = _inventory[f_index];
	item & dye_item = _inventory[d_index];
	if(fashion_item.type == -1 || dye_item.type == -1) return S2C::ERR_DYE_FAILED;
	
	//�����Ʒ�Ƿ�ʱװ ��Ⱦɫ��
	itemdataman & dataman =  world_manager::GetDataMan();
	DATA_TYPE dt;
	FASHION_ESSENCE * fess = (FASHION_ESSENCE*)dataman.get_data_ptr(fashion_item.type, ID_SPACE_ESSENCE,dt);
	if(fess == NULL || dt != DT_FASHION_ESSENCE) return S2C::ERR_DYE_FAILED;
	DYE_TICKET_ESSENCE * dess = (DYE_TICKET_ESSENCE*)dataman.get_data_ptr(dye_item.type, ID_SPACE_ESSENCE,dt);
	if(dess == NULL || dt != DT_DYE_TICKET_ESSENCE) return S2C::ERR_DYE_FAILED;

	if (dess->usage != 0) return S2C::ERR_DYE_FAILED;

	//���ʱװ�Ƿ����Ⱦɫ
	if(fess->require_dye_count <= 0) return S2C::ERR_FASHION_CAN_NOT_BE_DYED;

	//���Ⱦɫ���Ƿ��㹻
	if(!_inventory.IsItemExist(dye_item.type, fess->require_dye_count)) return S2C::ERR_DYE_NOT_ENOUGH;

	float h,s,v;
	h = abase::Rand(dess->h_min, dess->h_max);
	s = abase::Rand((float)pow(dess->s_min,2), (float)pow(dess->s_max,2));
	v = abase::Rand((float)pow(dess->v_min,3), (float)pow(dess->v_max,3));
	
	s = sqrtf(s);
	v = pow(v,1.0/3); 
	
	if(s > dess->s_max) s = dess->s_max; else if(s < dess->s_min) s = dess->s_min;
	if(v > dess->v_max) v = dess->v_max; else if(v < dess->v_min) v = dess->v_min;

	int color = hsv2rgb(h,s,v);

	unsigned short r = ((color >> 16) >> 3) & 0x1F;
	unsigned short g = ((color >> 8) >> 3) & 0x1F;
	unsigned short b = (color >> 3) & 0x1F;
    fashion_item.DyeItem(((r << 10) | (g << 5) | b) & 0x7FFF);

	//ɾ����Ʒ
	RemoveItems(dye_item.type,fess->require_dye_count, S2C::DROP_TYPE_USE, true);

	//֪ͨ�ͻ�������Ʒ����
	PlayerGetItemInfo(IL_INVENTORY, f_index);

	//��ȷ����
	return 0;
}

int 
gplayer_imp::DyeSuitFashionItem(unsigned char inv_idx_body,unsigned char inv_idx_leg,unsigned char inv_idx_foot,unsigned char inv_idx_wrist,unsigned char inv_idx_dye)
{
	//���ʱװ����������Ⱦɫ����		
	if(inv_idx_body == 255 && inv_idx_leg == 255 && inv_idx_foot == 255 && inv_idx_wrist == 255) return S2C::ERR_DYE_FAILED;
	
	int require_dye_count = 0;
	int count;
	if(inv_idx_body != 255)
	{
		count = GetRequireDyeCount(inv_idx_body,item::EQUIP_INDEX_FASHION_BODY);
		if(count <= 0) return S2C::ERR_DYE_FAILED;
		require_dye_count += count;
	}
	if(inv_idx_leg != 255)
	{
		count = GetRequireDyeCount(inv_idx_leg,item::EQUIP_INDEX_FASHION_LEG);
		if(count <= 0) return S2C::ERR_DYE_FAILED;
		require_dye_count += count;
	}
	if(inv_idx_foot != 255)
	{
		count = GetRequireDyeCount(inv_idx_foot,item::EQUIP_INDEX_FASHION_FOOT);
		if(count <= 0) return S2C::ERR_DYE_FAILED;
		require_dye_count += count;
	}
	if(inv_idx_wrist != 255)
	{
		count = GetRequireDyeCount(inv_idx_wrist,item::EQUIP_INDEX_FASHION_WRIST);
		if(count <= 0) return S2C::ERR_DYE_FAILED;
		require_dye_count += count;
	}

	//���Ⱦɫ������
	if(inv_idx_dye >= _inventory.Size()) return S2C::ERR_DYE_FAILED;
	item & dye_item = _inventory[inv_idx_dye];
	if(dye_item.type <= 0) return S2C::ERR_DYE_FAILED;
	DATA_TYPE dt;
	DYE_TICKET_ESSENCE * dess = (DYE_TICKET_ESSENCE*)world_manager::GetDataMan().get_data_ptr(dye_item.type, ID_SPACE_ESSENCE,dt);
	if(dess == NULL || dt != DT_DYE_TICKET_ESSENCE) return S2C::ERR_DYE_FAILED;

	if (dess->usage != 0) return S2C::ERR_DYE_FAILED;

	require_dye_count = require_dye_count*2;
	if(!CheckItemExist(dye_item.type, require_dye_count)) return S2C::ERR_DYE_NOT_ENOUGH;

	//����Ⱦɫ
	float h,s,v;
	h = abase::Rand(dess->h_min, dess->h_max);
	s = abase::Rand((float)pow(dess->s_min,2), (float)pow(dess->s_max,2));
	v = abase::Rand((float)pow(dess->v_min,3), (float)pow(dess->v_max,3));
	
	s = sqrtf(s);
	v = pow(v,1.0/3); 
	
	if(s > dess->s_max) s = dess->s_max; else if(s < dess->s_min) s = dess->s_min;
	if(v > dess->v_max) v = dess->v_max; else if(v < dess->v_min) v = dess->v_min;

	int color = hsv2rgb(h,s,v);

	unsigned short r = ((color >> 16) >> 3) & 0x1F;
	unsigned short g = ((color >> 8) >> 3) & 0x1F;
	unsigned short b = (color >> 3) & 0x1F;
	
	//ɾ��Ⱦɫ��
	RemoveItems(dye_item.type,require_dye_count, S2C::DROP_TYPE_USE, true);

	//Ⱦɫ��֪ͨ�ͻ���
	if(inv_idx_body != 255)
	{
        _inventory[inv_idx_body].DyeItem(((r << 10) | (g << 5) | b) & 0x7FFF);
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_body);
	}
	if(inv_idx_leg != 255)
	{
        _inventory[inv_idx_leg].DyeItem(((r << 10) | (g << 5) | b) & 0x7FFF);
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_leg);
	}
	if(inv_idx_foot != 255)
	{
        _inventory[inv_idx_foot].DyeItem(((r << 10) | (g << 5) | b) & 0x7FFF);
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_foot);
	}
	if(inv_idx_wrist != 255)
	{
        _inventory[inv_idx_wrist].DyeItem(((r << 10) | (g << 5) | b) & 0x7FFF);
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_wrist);
	}

	return 0;
}

int 
gplayer_imp::GetRequireDyeCount(unsigned char inv_idx, int equip_index)
{
	if(inv_idx >= _inventory.Size()) return -1;
	
	item & fashion_item = _inventory[inv_idx];
	if(fashion_item.type <= 0) return -1;
	if(!fashion_item.CheckEquipPostion(equip_index)) return -1;
	
	DATA_TYPE dt;
	FASHION_ESSENCE * fess = (FASHION_ESSENCE*)world_manager::GetDataMan().get_data_ptr(fashion_item.type, ID_SPACE_ESSENCE,dt);
	if(fess == NULL || dt != DT_FASHION_ESSENCE) return -1;
	return fess->require_dye_count;
}

void 
gplayer_imp::FindCheater2()
{
	if(_cheat_report) return;
	_cheat_report = 1;
	GMSV::ReportCheater2Gacd(_parent->ID.id, 1, NULL,0);

	A3DVECTOR pos = _parent->pos;
	GLog::log(LOG_INFO,"�û�%d����ʹ���ѻ���ң���ǰ����(%f,%f,%f)���ѻ㱨������ҷ�����", _parent->ID.id,pos.x, pos.y, pos.z);
}

void 
gplayer_imp::FindCheater(int type, bool noreport)
{
	if(!world_manager::AntiCheat()) return;
	_cooldown.SetCoolDown(COOLDOWN_INDEX_CHEATER,3600*1000);
	if(_cheat_mode) return ;	//�Ѿ���������״̬�Ͳ��ڼ�����
	
	_cheat_mode = abase::Rand(80, 127);
	__PRINTF("����%d���� %d\n", _parent->ID.id, type);
	if(!noreport)
	{
		GMSV::ReportCheater2Gacd(_parent->ID.id, type, NULL,0);
	}
	A3DVECTOR pos = _parent->pos;
	GLog::log(LOG_INFO,"�û�%d����ʹ�����׹���%d��ǰ����(%f,%f,%f)%d������ͷ�״̬", _parent->ID.id, type, pos.x, pos.y, pos.z, _cheat_mode);

}

void 
gplayer_imp::PunishCheat()
{
	//�ͷ������� 
	_cheat_punish = 1;
	__PRINTF("%d���׳ͷ���ʼ\n", _parent->ID.id);
}

void 
gplayer_imp::WallowControl(int level, int p_time, int l_time, int h_time, int reason)
{
	if(level < 0 || level >= anti_wallow::MAX_WALLOW_LEVEL - 1) return ;
	_wallow_level = level + 1;
	_runner->player_wallow_info(level, p_time, l_time, h_time, reason);
}

bool gplayer_imp::CheckPlayerAutoSupport()
{
	for(unsigned int i = 0; i < _inventory.Size(); ++i)
	{
		int item_type = _inventory[i].type;
		if(item_type == -1) continue;
		if((item_type == AUTO_SUPPORT_STONE1) || (item_type == AUTO_SUPPORT_STONE2) 
			|| (item_type == AUTO_SUPPORT_STONE3) || (item_type == AUTO_SUPPORT_STONE4))
		{
			return true;
		}
	}
	return false;
}

void gplayer_imp::OnAntiCheatAttack(float rate)
{
	if(!world_manager::GetWorldLimit().anti_cheat) return;
	if(abase::RandUniform() < rate)
	{
		if(CheckCoolDown(COOLDOWN_INDEX_ANTI_CHEAT))
		{
			SetCoolDown(COOLDOWN_INDEX_ANTI_CHEAT,20*60*1000);
			
			bool need_anticheat = !CheckPlayerAutoSupport();
			
			if(need_anticheat)
			{
				GMSV::TriggerQuestion2Gacd(_parent->ID.id);
				__PRINTF("�û�%d����Ҵ�����\n",_parent->ID.id);
			}
		}
	}
}

void gplayer_imp::QuestionBonus()
{
	float  val= abase::RandUniform();
	//__PRINTF("�û�%d����Ҵ������ˣ�����%d\n",_parent->ID.id,val < 0.5f?0:1);
	GLog::log(GLOG_INFO,"�û�%d����Ҵ������ˣ�����%d\n",_parent->ID.id,val < 0.5f?0:1);
	if(val < 0.5f)
	{
		int exp = int(_basic.level * (20.f+ _basic.level*0.1f)*(5.0/75.f)) * 100;
		ReceiveTaskExp(exp, 0);
	}
	else
	{
		int sp = int(_basic.level * (20.f+ _basic.level*0.1f)*(5.0/75.f)) * 25;
		ReceiveTaskExp(0, sp);
	}
}

bool    
gplayer_imp::CheckAndSetCoolDown(int idx, int msec)
{       
	if(!_cooldown.TestCoolDown(idx)) return false;
	_cooldown.SetCoolDown(idx,msec);
	_runner->set_cooldown(idx, msec);
	return true;
}       

void
gplayer_imp::DoTeamRelationTask(int reason) 
{
	//���봦�ڶ�����ֻ�жӳ��ܷ���������
	if(!IsInTeam() || !IsTeamLeader()) return; 
	if(GetTeamMemberNum() <= 1) return ;
	//�������������1������ȴ
	if(!CheckAndSetCoolDown(COOLDOWN_INDEX_TEAM_RELATION, 1000)) return ;

	class TeamRelationExec : public ONET::Thread::Runnable
	{       
		int _leader;
		int _reason;
		world * _plane; 
		abase::vector<int , abase::fast_alloc<> > _list;
		public: 
		TeamRelationExec(gplayer_imp * pImp, int reason, const XID * list, size_t count)
		{       
			_plane = pImp->_plane;
			_leader = pImp->GetParent()->ID.id;
			_reason = reason; 
			_list.reserve(count);
			for(size_t i = 0; i < count ; i ++) 
			{       
				_list.push_back(list[i].id);
			}       
		}       
		virtual void Run()
		{       
			OnRun();
			delete this;
		}       

		bool OnRun() 
		{       
			//���ȸ��������˵�ID�õ����е�pPlayer
			abase::vector<gplayer *, abase::fast_alloc<> > plist;
			abase::vector<int *, abase::fast_alloc<> > llist;
			gplayer * pLeader = NULL; 
			plist.reserve(_list.size());
			llist.reserve(_list.size());
			world_manager * pManager = _plane->GetWorldManager();
			for(size_t i = 0; i < _list.size();i ++)
			{       
				int foo;
				gplayer *pPlayer = pManager->FindPlayer(_list[i],foo);
				if(pPlayer == NULL) return false;       //�Ҳ���ָ���Ķ���
				plist.push_back(pPlayer);
				llist.push_back(&pPlayer->spinlock);
				if(_list[i] == _leader) pLeader = pPlayer;
			}       
			if(pLeader == NULL) return false;       //�Ҳ��������ߣ��ӳ���

			std::sort(llist.begin(), llist.end());

			//����������� 
			for(size_t i = 0; i < llist.size();i ++)
			{
				mutex_spinlock(llist[i]);
			}

			try
			{
				//��������˵�״̬λ���Ƿ���ȷ
				A3DVECTOR pos = pLeader->pos;
				for(size_t i = 0; i < _list.size();i ++)
				{
					gplayer * pPlayer = plist[i];
					if(pPlayer->ID.id != _list[i])  throw -3;
					if(!pPlayer->IsActived() ||
							pPlayer->IsZombie() ||
							pPlayer->login_state != gplayer::LOGIN_OK) throw -4;
					if(pPlayer->pos.squared_distance(pos) > 30.f*30.f) throw -5;
					if(!pPlayer->imp) throw -6;
					if(!pPlayer->imp->CanTeamRelation()) throw -7;
					if(!pPlayer->imp->GetRunTimeClass()->IsDerivedFrom(gplayer_imp::GetClass()))
						throw -8;

				}

				//������״̬�Ƿ����
				gplayer_imp * pLmp = (gplayer_imp *) pLeader->imp;
				if(!pLmp->IsInTeam() || !pLmp->IsTeamLeader()) throw -9;
				for(size_t i = 0; i < _list.size();i ++)
				{
					gplayer * pPlayer = plist[i];
					if(pPlayer == pLeader) continue;
					if(!pLmp->IsMember(pPlayer->ID)) throw -10;
				}

				PlayerTaskTeamInterface TaskTeam;
				//ʹ������ļ��ӿڽ��м��
				for(size_t i = 0; i < _list.size();i ++)
				{
					gplayer * pPlayer = plist[i];
					gplayer_imp * pImp = (gplayer_imp *) pPlayer->imp;
					PlayerTaskInterface TaskIf(pImp);
					if(!TaskIf.OnCheckTeamRelationship(_reason,&TaskTeam))
					{
						throw -11;
					}
				}

				//���м�鶼ͨ���ˣ����������ִ�нӿ�
				for(size_t i = 0; i < _list.size();i ++)
				{
					gplayer * pPlayer = plist[i];
					gplayer_imp * pImp = (gplayer_imp *) pPlayer->imp;
					PlayerTaskInterface TaskIf(pImp);
					if(!TaskIf.OnCheckTeamRelationship(_reason,&TaskTeam))
					{
						throw -11;
					}
				}

				//���м�鶼ͨ���ˣ����������ִ�нӿ�
				for(size_t i = 0; i < _list.size();i ++)
				{
					gplayer * pPlayer = plist[i];
					gplayer_imp * pImp = (gplayer_imp *) pPlayer->imp;
					PlayerTaskInterface TaskIf(pImp);
					TaskIf.OnCheckTeamRelationshipComplete(_reason,&TaskTeam);
				}

				//����ִ�ж�����߼���ϵ
				TaskTeam.Execute(plist.begin(), plist.size());
			}catch(int e)
			{
				__PRINTF("��ӹ�ϵ������ʧ�ܣ������:%d\n", e);
			}


			//�����������
			for(size_t i = 0; i < llist.size();i ++)
			{
				mutex_spinunlock(llist[i]);
			}
			return true;
		}
	};

	XID list[TEAM_MEMBER_CAPACITY];
	int count = GetMemberList(list);
	ONET::Thread::Pool::AddTask( new TeamRelationExec(this, reason, list, count));
}

bool
gplayer_imp::CanTeamRelation()
{
	if(_player_state != PLAYER_STATE_NORMAL) return false;
	return true;
}

int  
gplayer_imp::RefineTransmit(size_t src_index, size_t dest_index)
{
	//�����Ʒ�Ƿ�����Լ��Ƿ�Ϸ�
	if(src_index >= _inventory.Size() || dest_index >= _inventory.Size()) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	item & src_item = _inventory[src_index];
	item & dest_item = _inventory[dest_index];
	if(dest_item.type == -1 || src_item.type == -1) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(dest_item.body == NULL || src_item.body == NULL) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	int src_bindlevel = 0;
	int dest_bindlevel = 0;
	if(src_item.proc_type & item::ITEM_PROC_TYPE_BIND)
	{
		if(src_item.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE)
		{
			src_bindlevel = 1;
		}
		else
		{
			src_bindlevel = 2;
		}
	}
	if(dest_item.proc_type & item::ITEM_PROC_TYPE_BIND)
	{
		if(dest_item.proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE)
		{
			dest_bindlevel = 1;
		}
		else
		{
			dest_bindlevel = 2;
		}
	}
	//�������װ�������˺�һ״̬����
	if(dest_bindlevel <  src_bindlevel)
	{
		return S2C::ERR_TRANSMIT_REFINE_NEED_BIND;
	}

	//�������װ�����ǿ��Խ��о�������
	int material_need;
	int dest_refine_addon = world_manager::GetDataMan().get_item_refine_addon(dest_item.type, material_need);
	if(dest_refine_addon <= 0 || material_need <= 0) return S2C::ERR_DEST_CAN_NOT_TRANSMIT_REFINE;
	int src_refine_addon = world_manager::GetDataMan().get_item_refine_addon(src_item.type, material_need);
	if(src_refine_addon <= 0 || material_need <= 0) return S2C::ERR_DEST_CAN_NOT_TRANSMIT_REFINE;

	//�������װ���ľ����ȼ���ƥ���
	int dest_level = dest_item.body->GetRefineLevel(dest_refine_addon);
	int src_level = src_item.body->GetRefineLevel(src_refine_addon);
	if(dest_level < 0  || src_level < 0 || src_level > 12) return S2C::ERR_CAN_NOT_TRANSMIT_REFINE;
	if(src_level == 0 || dest_level >= src_level) return S2C::ERR_LOW_LEVEL_TRANSMIT_REFINE;
	
	//���Ǭ��ʯ�������Ƿ��㹻
	const int need_material[] = {1,5,12,28,60,80,100,150,250,400,650,1000};
	if(!CheckItemExist(ALLSPARK_ID,	need_material[src_level -1])) return S2C::ERR_TRANSMIT_REFINE_NO_MATERIAL;
	
	//����˫�����¾����ȼ�
	int n_src_level = src_item.body->SetRefineLevel(src_refine_addon, 0);
	int n_dest_level = dest_item.body->SetRefineLevel(dest_refine_addon, src_level);
	
	//����־
	GLog::log(GLOG_INFO,"���%dִ���˾�����ת��������(%d[%d->%d])��(%d[%d->%d])",_parent->ID.id, 
			src_item.type, src_level, n_src_level, 
			dest_item.type,dest_level,n_dest_level);

	//�۳�Ǭ��ʯ
	PlayerTaskInterface TaskIf(this);
	TaskIf.TakeAwayCommonItem(ALLSPARK_ID, need_material[src_level - 1]);
	
	//֪ͨ�ͻ������ݸ���
	PlayerGetItemInfo(IL_INVENTORY,src_index);
	PlayerGetItemInfo(IL_INVENTORY,dest_index);
	return 0;
}

bool
gplayer_imp::ForeignDoShoppingStep1(int goods_id, size_t goods_index, size_t goods_slot)
{
	//������Ҫ���Ӹ�����Թ����״̬
	if(_player_state != PLAYER_SIT_DOWN && _player_state != PLAYER_STATE_NORMAL && _player_state != PLAYER_STATE_BIND) return false;
	if(!GetInventory().GetEmptySlotCount())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	netgame::mall & shop = world_manager::GetPlayerMall();
	if(goods_slot >= netgame::mall::MAX_ENTRY)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return true;
	}
	netgame::mall::node_t node;
	if(!shop.QueryGoods(goods_index,node) || node.goods_id != goods_id)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return true;
	}
	if(node.gift_id > 0)	//�����ݲ�֧����һ��һ
	{
		return false;
	}

	if(!node.check_owner(0))
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return true;
	}

	if(node.entry[goods_slot].cash_need <= 0)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return true;
	}
	if(node.entry[goods_slot].expire_time&&node.entry[goods_slot].expire_type!=netgame::mall::EXPIRE_TYPE_TIME)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
	}
	int name_len =0;
	unsigned char *goods_name = world_manager::GetDataMan().get_item_name(node.goods_id,name_len);
	if(goods_name  == NULL)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return false;
	}
	
	//����Э���Delivery
	if(world_manager::GetWorldParam().korea_shop)
	{
		GNET::SendBillingRequest(_parent->ID.id,node.goods_id, node.goods_count,node.entry[goods_slot].expire_time, node.entry[goods_slot].cash_need);
	}
	else if(world_manager::GetWorldParam().southamerican_shop)
	{
		int unit_price = node.entry[goods_slot].cash_need/node.goods_count; 
		GNET::SendBillingConfirmSA(_parent->ID.id,node.goods_id,node.goods_count,goods_name,name_len,node.entry[goods_slot].expire_time, unit_price);
	}
	return true;
}

bool
gplayer_imp::ForeignDoShoppingStep2(int item_id, size_t count, int expire_time, size_t cash_need)
{
	//������Ҫ���Ӹ�����Թ����״̬
	if(_player_state != PLAYER_SIT_DOWN && _player_state != PLAYER_STATE_NORMAL && _player_state != PLAYER_STATE_BIND) return false;
	if(!GetInventory().GetEmptySlotCount())
	{
		return false;
	}
	
	const item_data * pItem = (const item_data*)world_manager::GetDataMan().get_item_for_sell(item_id);
	if(!pItem) return false;
	item_data * pItem2 = DupeItem(*pItem);
	int guid1 = 0;
	int guid2 = 0;
	int expire_date = 0;
	if(expire_time)  expire_date = g_timer.get_systime() + expire_time;
	if(pItem2->guid.guid1 != 0)
	{
		//�����Ҫ������GUID
		get_item_guid(pItem2->type, guid1,guid2);
		pItem2->guid.guid1 = guid1;
		pItem2->guid.guid2 = guid2;
	}

	pItem2->proc_type |= item::ITEM_PROC_TYPE_MALL;
	UpdateMallConsumptionShopping(pItem2->type, pItem2->proc_type, count, cash_need);

	int ocount = count;
	if((size_t)ocount > (size_t)pItem2->pile_limit) ocount = pItem2->pile_limit;
	int rst =_inventory.Push(*pItem2,ocount,expire_date);
	ASSERT( rst >= 0);
	_runner->obtain_item(item_id,pItem2->expire_date,count,_inventory[rst].count, 0,rst);
	//�������³�ʼ��һ�¿��ܵ�ʱװ
	_inventory[rst].InitFromShop();
    
	//��¼��־  
	_mall_order_id ++;
	GLog::formatlog("formatlog:gshop_trade:userid=%d:db_magic_number=%d:order_id=%d:item_id=%d:expire=%d:item_count=%d:cash_need=%d:guid1=%d:guid2=%d",
			_parent->ID.id,_db_user_id, _mall_order_id,item_id, expire_date,count,cash_need,guid1,guid2);
	world_manager::TestCashItemGenerated(item_id, count);
	FreeItem(pItem2);

	_mall_cash -= (int)cash_need;		//���ﲻ��ı�ʹ�ü�¼ ���� cash_offset��cash_usedӦʼ��Ϊ0
	/* $$$$$$$$$$$$$$ Ŀǰ���ײ����湺���б��� ��Ϊ����Ϊ�˱�̫�� ����������ʧ��
	_mall_invoice.push_back(netgame::mall_invoice(_mall_order_id, item_id, count,cash_need,expire_date, g_timer.get_systime(), guid1,guid2));*/
	_runner->player_cash(GetMallCash());

	if(world_manager::GetWorldParam().korea_shop)
	{
		GNET::SendBillingBalance(_parent->ID.id);
	}
	else if(world_manager::GetWorldParam().southamerican_shop)
	{
		GNET::SendBillingBalanceSA(_parent->ID.id);
	}
	GLog::log(GLOG_INFO,"�û�%d�ڰٱ�����%d��%d������%d�����ʣ��%d��",
			_parent->ID.id, count, item_id, cash_need, GetMallCash());
	return true;
}

bool gplayer_imp::PlayerGetMallItemPrice(int start_index, int end_index)	//lgc����������Ϊ0, ���ʾɨ��������,
{																			//����[start_index,end_index)�ڵ���Ʒ��ɨ��
	netgame::mall & __mall = world_manager::GetPlayerMall();	
	size_t __mall_goods_count = __mall.GetGoodsCount();
	int __group_id = __mall.GetGroupId();	//��ǰ�������趨��group_id
	
	if(start_index == 0 && end_index == 0)	//ɨ��ȫ��
	{
		start_index = 0;
		end_index = __mall_goods_count;
	}
	else
	{
		if(start_index < 0 || end_index <= 0 || (size_t)start_index >= __mall_goods_count || (size_t)end_index > __mall_goods_count || start_index >= end_index)
		{
			_runner->error_message(S2C::ERR_FATAL_ERR);
			return false;
		}
	}
	//ֻ��ɨ�跶Χ����ָ��ֵʱ��������ȴ
	if(end_index-start_index > 10 && !CheckCoolDown(COOLDOWM_INDEX_GET_MALL_PRICE)) return false;
	SetCoolDown(COOLDOWM_INDEX_GET_MALL_PRICE,GET_MALL_PRICE_COOLDOWN_TIME);
	
	//���ܷ����仯����Ʒ�б�	
	abase::vector<netgame::mall::index_node_t, abase::fast_alloc<> > & __limit_goods = __mall.GetLimitGoods();
	size_t __limit_goods_count = __limit_goods.size();
	
	time_t __time = time(NULL);
	packet_wrapper __h0(1024);
	int	__h0_count = 0; 
    using namespace S2C;
	
	ASSERT(netgame::mall::MAX_ENTRY == 4);
	for(size_t i=0; i<__limit_goods_count; i++)
	{
		int index = __limit_goods[i]._index;
		if(index < start_index)
			continue;
		if(index >= end_index)
			break;
		netgame::mall::node_t & node = __limit_goods[i]._node;
			
		//�ҵ���ǰ��Ч��group
		int active_group_id = 0;		
		if(node.group_active && __group_id != 0)
		{
			if(__group_id == node.entry[0].group_id || __group_id == node.entry[1].group_id || __group_id == node.entry[2].group_id || __group_id == node.entry[3].group_id)
				active_group_id = __group_id;	
		}
	
		if(!node.sale_time_active)		//ֻ�����õ�����ʱ��
		{
			//����Ч�ķ�Ĭ��group������	�����ͻ���
			ASSERT(node.group_active);
			if(active_group_id != 0)
			{
				for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
				{
					if(node.entry[j].cash_need <= 0)
						break;
					if(active_group_id == node.entry[j].group_id)	//�����������ҵ�һ��
					{
						CMD::Make<CMD::mall_item_price>::AddGoods(__h0, 
														index, 
														j, 
														node.goods_id, 
														node.entry[j].expire_type, 
														node.entry[j].expire_time, 
														node.entry[j].cash_need, 
														node.entry[j].status);
						__h0_count ++;
					}
				}
			}
		}
		else							//���ڷ����õ�����ʱ��
		{
			int available_permanent = -1;				//��Ч����������ʱ���slot������һ��
			int available_saletime = -1;				//��Ч�ķ���������ʱ���slot������һ��
			for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
			{
				if(node.entry[j].cash_need <= 0)
					break;
				if(node.entry[j].group_id == active_group_id && node.entry[j]._sale_time.CheckAvailable(__time))
				{
					if(node.entry[j]._sale_time.GetType() != netgame::mall::sale_time::TYPE_NOLIMIT)
					{
						available_saletime = j;
						break;		///ͬ����ֻ����һ������������ʱ�����㣬����ȷ������Ʒ��ǰ�ļ۸�
					}
					else
						available_permanent = j;	//���౻ִ��һ��	
				}			
			}
			if(available_saletime >= 0)		//ͬ����ֻ����һ������������ʱ�����㣬����ȷ������Ʒ��ǰ�ļ۸�
			{
				CMD::Make<CMD::mall_item_price>::AddGoods(__h0, 
														index, 
														available_saletime, 
														node.goods_id, 
														node.entry[available_saletime].expire_type, 
														node.entry[available_saletime].expire_time, 
														node.entry[available_saletime].cash_need, 
														node.entry[available_saletime].status);
				__h0_count ++;
			}
			else if(available_permanent >= 0 )	//ͬ��������������ʱ������
			{
				if(active_group_id != 0)	//ֻҪ��Ĭ��group�����ݲŻᷢ���ͻ���	
				{
					CMD::Make<CMD::mall_item_price>::AddGoods(__h0, 
																index, 
																available_permanent, 
																node.goods_id, 
																node.entry[available_permanent].expire_type, 
																node.entry[available_permanent].expire_time, 
																node.entry[available_permanent].cash_need, 
																node.entry[available_permanent].status);
					__h0_count ++;
				}
			}
			else		//��Ʒ�¼�,ֻ���ڿͻ��˵�gshop.data�д������õĹ���ʽʱ����Ҫ������Ʒ�¼�
			{
				if(active_group_id != 0)	//����0�������ǰ���Ѿ��жϿͻ��˿϶�û�����õĹ���ʽ
				{
					int m;
					for(int m=0; m<netgame::mall::MAX_ENTRY; m++)
					{
						if(node.entry[m].cash_need > 0 && node.entry[m].group_id == 0 
							&& node.entry[m]._sale_time.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
							break;
					}
					if(m < netgame::mall::MAX_ENTRY)
					{
						CMD::Make<CMD::mall_item_price>::AddGoods(__h0, 
																index, 
																0, 
																node.goods_id, 
																0, 
																0, 
																0, 
																0);
						__h0_count ++;
					}
				}
			}
		}//end of if(!sale_time_active)
	}
	
	packet_wrapper __h1(1024);
	CMD::Make<CMD::mall_item_price>::From(__h1, start_index, end_index, __h0_count);
	if(__h0_count > 0)
		__h1.push_back(__h0.data(), __h0.size());
	gplayer * pPlayer = (gplayer*)_parent;
	send_ls_msg(pPlayer,__h1);
	
	return true;
}

bool 
gplayer_imp::PlayerEquipTrashboxFashionItem(unsigned char trash_idx_body,unsigned char trash_idx_leg,unsigned char trash_idx_foot,unsigned char trash_idx_wrist,unsigned char trash_idx_head,unsigned char trash_idx_weapon)
{
	if(!_cooldown.TestCoolDown(COOLDOWN_INDEX_EQUIP_FASHION_ITEM))	return false;
	
	if(_lock_equipment)
	{	
		_runner->error_message(S2C::ERR_EQUIPMENT_IS_LOCKED);
		return false;
	}

	item_list & box3 = GetTrashInventory(IL_TRASH_BOX3);
	if(!box3.Size()) return false;
	
	if(trash_idx_body == 255 && trash_idx_leg == 255 && trash_idx_foot == 255 && trash_idx_wrist == 255 && trash_idx_head == 255 && trash_idx_weapon == 255) return false;

	if(trash_idx_body != 255 && !CheckEquipTrashboxItem(box3, trash_idx_body, item::EQUIP_INDEX_FASHION_BODY)) return false;
	if(trash_idx_leg != 255 && !CheckEquipTrashboxItem(box3, trash_idx_leg, item::EQUIP_INDEX_FASHION_LEG)) return false;
	if(trash_idx_foot != 255 && !CheckEquipTrashboxItem(box3, trash_idx_foot, item::EQUIP_INDEX_FASHION_FOOT)) return false;
	if(trash_idx_wrist != 255 && !CheckEquipTrashboxItem(box3, trash_idx_wrist, item::EQUIP_INDEX_FASHION_WRIST)) return false;
	if(trash_idx_head != 255 && !CheckEquipTrashboxItem(box3, trash_idx_head, item::EQUIP_INDEX_FASHION_HEAD)) return false;
	if(trash_idx_weapon != 255 && !CheckEquipTrashboxItem(box3, trash_idx_weapon, item::EQUIP_INDEX_FASHION_WEAPON)) return false;

	//��λ�ò�����ͬ
	unsigned char empty_idx[6] = {0};
	int empty_idx_cnt = 0;
	if(trash_idx_body != 255 && box3[trash_idx_body].type <= 0) empty_idx[empty_idx_cnt++] = trash_idx_body;
	if(trash_idx_leg != 255 && box3[trash_idx_leg].type <= 0) empty_idx[empty_idx_cnt++] = trash_idx_leg;
	if(trash_idx_foot != 255 && box3[trash_idx_foot].type <= 0) empty_idx[empty_idx_cnt++] = trash_idx_foot;
	if(trash_idx_wrist != 255 && box3[trash_idx_wrist].type <= 0) empty_idx[empty_idx_cnt++] = trash_idx_wrist;
	if(trash_idx_head != 255 && box3[trash_idx_head].type <= 0) empty_idx[empty_idx_cnt++] = trash_idx_head;
	if(trash_idx_weapon != 255 && box3[trash_idx_weapon].type <= 0) empty_idx[empty_idx_cnt++] = trash_idx_weapon;

	for(int i=0; i<empty_idx_cnt-1; i++)
	{
		for(int j=i+1; j<empty_idx_cnt; j++)
			if(empty_idx[i] == empty_idx[j]) return false;
	}

	if(trash_idx_body != 255) PlayerEquipTrashboxItem(IL_TRASH_BOX3,trash_idx_body,item::EQUIP_INDEX_FASHION_BODY);	
	if(trash_idx_leg != 255) PlayerEquipTrashboxItem(IL_TRASH_BOX3,trash_idx_leg,item::EQUIP_INDEX_FASHION_LEG);	
	if(trash_idx_foot != 255) PlayerEquipTrashboxItem(IL_TRASH_BOX3,trash_idx_foot,item::EQUIP_INDEX_FASHION_FOOT);	
	if(trash_idx_wrist != 255) PlayerEquipTrashboxItem(IL_TRASH_BOX3,trash_idx_wrist,item::EQUIP_INDEX_FASHION_WRIST);	
	if(trash_idx_head != 255) PlayerEquipTrashboxItem(IL_TRASH_BOX3,trash_idx_head,item::EQUIP_INDEX_FASHION_HEAD);	
	if(trash_idx_weapon != 255) PlayerEquipTrashboxItem(IL_TRASH_BOX3,trash_idx_weapon,item::EQUIP_INDEX_FASHION_WEAPON);	

	SetCoolDown(COOLDOWN_INDEX_EQUIP_FASHION_ITEM,EQUIP_FASHION_ITEM_COOLDOWN_TIME);
	return true;	
}

bool 
gplayer_imp::CheckEquipTrashboxItem(item_list & trashbox, unsigned char trash_idx,unsigned char equip_idx)
{
	if(trash_idx >= trashbox.Size()) return false;
	item & it = trashbox[trash_idx]; 
	if(it.type <= 0)
	{
		if(_equipment[equip_idx].type <= 0) return false;
	}
	else
	{
		if(!it.CheckEquipPostion(equip_idx)) return false;
		if(!it.CanActivate(_equipment,equip_idx,this)) return false;
	}
	return true;
}

bool 
gplayer_imp::PlayerEquipTrashboxItem(int where, unsigned char trash_idx,unsigned char equip_idx)
{
	/*
	 �������ֿ�װ����Ʒ��Ŀǰֻ֧��ʱװ�Ϳ���
	 */
	if(!IsPortableTrashBox(where)) return false;
	item_list & trashbox = GetTrashInventory(where);

	if(trash_idx >= trashbox.Size() || equip_idx >= _equipment.Size()) return false;

	if(_lock_equipment) return false;

	item & it1 = trashbox[trash_idx];
	item & it2 = _equipment[equip_idx];
	bool type1 = (it1.type == -1);
	bool type2 = (it2.type == -1);
	if(type1 && type2) return false;
	
	if(!type1)
	{
		//�ֿ����ǿ�
		if(!it1.CheckEquipPostion(equip_idx)) return false;
		//���Ƽ���ʱ������ͳ������Ϊ��ֹ�¿��Ʋ����㼤���������Ȱ�ԭ����ȡ������
		bool need_deactivate = (equip_idx >= item::EQUIP_INDEX_GENERALCARD1 && equip_idx <= item::EQUIP_INDEX_GENERALCARD6 && !type2);
		if(need_deactivate) it2.Deactivate(item::BODY, equip_idx, this);
		if(!it1.CanActivate(_equipment,equip_idx,this))
		{
			if(need_deactivate) it2.Activate(item::BODY, _equipment, equip_idx, this);
			return false;
		}
		
		//����
		item temp1,temp2;
		trashbox.Remove(trash_idx, temp1);
		_equipment.Remove(equip_idx, temp2);

		_equipment.Put(equip_idx,temp1);
		trashbox.Put(trash_idx,temp2);
		temp1.Clear();
		temp2.Clear();

		RefreshEquipment();
		
		//�����ɹ������һ���Ƿ���Ҫ���а󶨲���
		item & it = _equipment[equip_idx];
		ASSERT(it.type != -1);
		bool notify_equip_item = false;
		if(it.proc_type & item::ITEM_PROC_TYPE_BIND2)
		{
			//��������������а�
			it.proc_type |= item::ITEM_PROC_TYPE_NODROP |
				item::ITEM_PROC_TYPE_NOTHROW|
				item::ITEM_PROC_TYPE_NOSELL |
				item::ITEM_PROC_TYPE_NOTRADE|
				item::ITEM_PROC_TYPE_BIND;

			it.proc_type &= ~(item::ITEM_PROC_TYPE_BIND2);
			notify_equip_item = true;
			GLog::log(LOG_INFO,"�û�%dװ������Ʒ%d, GUID(%d,%d)",_parent->ID.id,it.type,it.guid.guid1,it.guid.guid2);

			UpdateMallConsumptionBinding(it.type, it.proc_type, it.count);
		}
		
		_runner->equip_trashbox_item(where,trash_idx,equip_idx);
		int id1 = it.type | it.GetIdModify();
		CalcEquipmentInfo();
		_runner->equipment_info_changed(1ULL<<equip_idx , 0, &id1,sizeof(id1));//�˺���ʹ����CalcEquipmentInfo�Ľ��
		
		if(notify_equip_item)
		{
			PlayerGetItemInfo(IL_EQUIPMENT,equip_idx);
		}
		IncEquipChangeFlag();
		IncTrashBoxChangeCounter();
		return true;
	}
	
	//�ֿ���Ϊ�գ�����ԭ���ģ���ˢ��װ��
	item  temp;
	_equipment.Remove(equip_idx,temp);
	bool bRst = trashbox.Put(trash_idx,temp);
	ASSERT(bRst);
	if(bRst)
	{
		temp.Clear();
	}
	else
	{
		//��¼������־
		GLog::log(GLOG_ERR,"װ����Ʒʱ������������");
		temp.Release();
	}
	RefreshEquipment();
	_runner->equip_trashbox_item(where,trash_idx,equip_idx);
	CalcEquipmentInfo();
	_runner->equipment_info_changed(0,1ULL<<equip_idx , 0, 0); //�˺���ʹ����CalcEquipmentInfo�Ľ��
	IncEquipChangeFlag();
	IncTrashBoxChangeCounter();
	return true;
}

void 
gplayer_imp::PlayerCheckSecurityPasswd(const char * passwd, size_t passwd_size)	
{
	if(!_trashbox.CheckPassword(passwd, passwd_size))
	{
		_runner->error_message(S2C::ERR_PASSWD_NOT_MATCH);
		return;
	}
	SetSecurityPasswdChecked(true);
}

void 
gplayer_imp::PlayerNotifyForceAttack(char force_attack, char refuse_bless)
{
	_force_attack = force_attack & C2S::FORCE_ATTACK_MASK_ALL;
	_refuse_bless = refuse_bless & C2S::REFUSE_BLESS_MASK_ALL;
}

bool
gplayer_imp::PlayerDoDividendShopping(size_t goods_count,const int * order_list, int shop_tid)
{
	//������Ҫ���Ӹ�����Թ����״̬
	if(_player_state != PLAYER_SIT_DOWN && _player_state != PLAYER_STATE_NORMAL && _player_state != PLAYER_STATE_BIND) return false;

	if(goods_count == 0)
	{
		return false;
	}
	if(goods_count > _inventory.Size() || !InventoryHasSlot(goods_count))
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}
	int gifts_count = 0;
	
	netgame::mall & shop = world_manager::GetPlayerMall2();
	int __group_id = shop.GetGroupId();	//��ǰ�������趨��group_id,lgc
	time_t __time = time(NULL);			//
	netgame::mall_order  order(-1);
	
	ASSERT(netgame::mall::MAX_ENTRY == 4);
	size_t offset = 0;
	for(size_t i = 0; i < goods_count; i ++, offset += sizeof(C2S::CMD::dividend_mall_shopping::__entry) / sizeof(int))
	{
		int id = order_list[offset];
		size_t index = order_list[offset +1];
		size_t slot = order_list[offset +2];
		if(slot >= netgame::mall::MAX_ENTRY)
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}
		netgame::mall::node_t node;
		if(!shop.QueryGoods(index,node) || node.goods_id != id)
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}

		if(!node.check_owner(shop_tid))
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}

		if(node.entry[slot].cash_need <= 0)
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return true;
		}
		//lgc	
		//�ҵ���ǰ��Ч��group
		int active_group_id = 0;
		if(node.group_active && __group_id != 0)
		{
			if(__group_id == node.entry[0].group_id || __group_id == node.entry[1].group_id || __group_id == node.entry[2].group_id || __group_id == node.entry[3].group_id)
				active_group_id = __group_id;	
		}

		if(node.sale_time_active)
		{	
			if(node.entry[slot].group_id == active_group_id && node.entry[slot]._sale_time.CheckAvailable(__time))	
			{
				//���playerѡ���slot�����õ����۷�ʽ������Ҫɨ�赱ǰ��Ч���ڣ����Ƿ񻹴��ڷ��������۷�ʽ�����
				if(node.entry[slot]._sale_time.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
				{
					for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
					{
						if(node.entry[j].cash_need <= 0)
							break;
						if(node.entry[j].group_id == active_group_id 
									&& node.entry[j]._sale_time.GetType() != netgame::mall::sale_time::TYPE_NOLIMIT
									&& node.entry[j]._sale_time.CheckAvailable(__time))
						{
							_runner->dividend_mall_item_buy_failed(index, 0);
							return false;
						}
					}
				}				
			}
			else
			{
				_runner->dividend_mall_item_buy_failed(index, 0);
				return false;
			}
		}
		else if(node.entry[slot].group_id != active_group_id)
		{
			_runner->dividend_mall_item_buy_failed(index, 0);
			return false;
		}

		if(node.gift_id > 0) gifts_count ++;  //ͳ����Ʒ��
		
		order.AddGoods(node.goods_id, node.goods_count,node.entry[slot].cash_need, node.entry[slot].expire_time,node.entry[slot].expire_type,node.gift_id,node.gift_count,node.gift_expire_time,node.gift_log_price);

	}
	if(_dividend_mall_info.GetDividend() < order.GetPointRequire())
	{
		//no engouh mall cash 
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return true;
	}
	if(!InventoryHasSlot(goods_count + gifts_count))
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		return false;
	}

	int total_cash = _dividend_mall_info.GetDividend();
	int cash_used = 0;
	//��Ǯ�㹻�� ��ʼ������Ʒ
	int cur_t = g_timer.get_systime();
	int self_id = GetParent()->ID.id;
	for(size_t i = 0; i < goods_count; i ++)
	{
		int id;
		int count;
		int point;
		int expire_time;
		int expire_type;
		int gift_id;
		int gift_count;
		int gift_expire_time;
		int gift_log_price;
		bool bRst = order.GetGoods(i, id, count,point, expire_time,expire_type,gift_id,gift_count,gift_expire_time,gift_log_price);
		if(bRst)
		{
			//������Ʒ����Ʒ��log�۸�
			int log_price1 = point;
			int log_price2 = 0;
			if(gift_id > 0 && gift_log_price > 0)
			{
				log_price1 = int((float)point*point/(point+gift_log_price));
				log_price2 = point - log_price1; 
			}

			const item_data * pItem = (const item_data*)world_manager::GetDataMan().get_item_for_sell(id);
			if(pItem)
			{
				item_data * pItem2 = DupeItem(*pItem);
				int expire_date = 0;
				if(expire_time) 
				{
					if(expire_type == netgame::mall::EXPIRE_TYPE_TIME)
					{
						//��Ч������һ������
						expire_date = cur_t + expire_time;
					}
					else
					{
						//��Ч���ǹ涨����ʧЧ
						expire_date = expire_time;
					}
				}
				int guid1 = 0;
				int guid2 = 0;
				if(pItem2->guid.guid1 != 0)
				{
					//void get_item_guid(int id, int & g1, int &g2);
					//�����Ҫ������GUID
					get_item_guid(pItem2->type, guid1,guid2);
					pItem2->guid.guid1 = guid1;
					pItem2->guid.guid2 = guid2;
				}

				int ocount = count;
				int rst =_inventory.Push(*pItem2,count,expire_date);
				ASSERT(rst >= 0 && count == 0);
				_runner->obtain_item(id,pItem2->expire_date,ocount,_inventory[rst].count, 0,rst);

				//�������³�ʼ��һ�¿��ܵ�ʱװ
				_inventory[rst].InitFromShop();

				total_cash -= log_price1;
				cash_used += log_price1;
				//��¼��־  
				GLog::formatlog("formatlog:gdividendshop_trade:userid=%d:db_magic_number=%d:item_id=%d:expire=%d:item_count=%d:dividend_need=%d:dividend_left=%d:guid1=%d:guid2=%d",
						self_id,_db_user_id,id, expire_date,ocount,log_price1,total_cash,guid1,guid2);
				
				world_manager::TestCashItemGenerated(id, ocount);
				FreeItem(pItem2);
			}
			else
			{
				//��¼������־
				GLog::log(GLOG_ERR,"�û�%d�ڹ�������̳���Ʒ%dʱ������Ʒʧ��",self_id, id);
			}

			//����Ϊ������Ʒ
			if(gift_id > 0)
			{
			const item_data * pGift = (const item_data*)world_manager::GetDataMan().get_item_for_sell(gift_id);
			if(pGift)
			{
				item_data * pGift2 = DupeItem(*pGift);
				int expire_date = 0;
				if(gift_expire_time) 
				{
					//��Ч������һ������
					expire_date = cur_t + gift_expire_time;
				}
				int guid1 = 0;
				int guid2 = 0;
				if(pGift2->guid.guid1 != 0)
				{
					//void get_item_guid(int id, int & g1, int &g2);
					//�����Ҫ������GUID
					get_item_guid(pGift2->type, guid1,guid2);
					pGift2->guid.guid1 = guid1;
					pGift2->guid.guid2 = guid2;
				}

				int ocount = gift_count;
				int rst =_inventory.Push(*pGift2,gift_count,expire_date);
				ASSERT(rst >= 0 && gift_count == 0);
				_runner->obtain_item(gift_id,expire_date,ocount,_inventory[rst].count, 0,rst);

				//�������³�ʼ��һ�¿��ܵ�ʱװ
                _inventory[rst].InitFromShop();
				
				total_cash -= log_price2;
				cash_used += log_price2;
				//��¼��־  
				GLog::formatlog("formatlog:gdividendshop_trade:userid=%d:db_magic_number=%d:item_id=%d:expire=%d:item_count=%d:dividend_need=%d:dividend_left=%d:guid1=%d:guid2=%d",
						self_id,_db_user_id,gift_id, expire_date,ocount,log_price2,total_cash,guid1,guid2);
				
				world_manager::TestCashItemGenerated(gift_id, ocount);
				FreeItem(pGift2);
			}
			else
			{
				//��¼������־
				GLog::log(GLOG_ERR,"�û�%d�ڹ�������̳���Ʒ%dʱ������Ʒ%dʧ��",self_id, id, gift_id);
			}
			}
		}
		else
		{
			ASSERT(false);
		}
	}

	bool rst = _dividend_mall_info.SpendDividend(cash_used);
	ASSERT(rst);
	_runner->player_dividend(_dividend_mall_info.GetDividend());

	GLog::log(GLOG_INFO,"�û�%d�ں����̳ǹ���%d����Ʒ������%d��ʣ��%d��",self_id,goods_count,cash_used,_dividend_mall_info.GetDividend());

	//���Ǽӿ�����ٶ� 
	return true;
}

bool gplayer_imp::PlayerGetDividendMallItemPrice(int start_index, int end_index)	//lgc����������Ϊ0, ���ʾɨ��������,
{																			//����[start_index,end_index)�ڵ���Ʒ��ɨ��
	netgame::mall & __mall = world_manager::GetPlayerMall2();	
	size_t __mall_goods_count = __mall.GetGoodsCount();
	int __group_id = __mall.GetGroupId();	//��ǰ�������趨��group_id
	
	if(start_index == 0 && end_index == 0)	//ɨ��ȫ��
	{
		start_index = 0;
		end_index = __mall_goods_count;
	}
	else
	{
		if(start_index < 0 || end_index <= 0 || (size_t)start_index >= __mall_goods_count || (size_t)end_index > __mall_goods_count || start_index >= end_index)
		{
			_runner->error_message(S2C::ERR_FATAL_ERR);
			return false;
		}
	}
	//ֻ��ɨ�跶Χ����ָ��ֵʱ��������ȴ
	if(end_index-start_index > 10 && !CheckCoolDown(COOLDOWM_INDEX_GET_DIVIDEND_MALL_PRICE)) return false;
	SetCoolDown(COOLDOWM_INDEX_GET_DIVIDEND_MALL_PRICE,GET_DIVIDEND_MALL_PRICE_COOLDOWN_TIME);
	
	//���ܷ����仯����Ʒ�б�	
	abase::vector<netgame::mall::index_node_t, abase::fast_alloc<> > & __limit_goods = __mall.GetLimitGoods();
	size_t __limit_goods_count = __limit_goods.size();
	
	time_t __time = time(NULL);
	packet_wrapper __h0(1024);
	int	__h0_count = 0; 
    using namespace S2C;
	
	ASSERT(netgame::mall::MAX_ENTRY == 4);
	for(size_t i=0; i<__limit_goods_count; i++)
	{
		int index = __limit_goods[i]._index;
		if(index < start_index)
			continue;
		if(index >= end_index)
			break;
		netgame::mall::node_t & node = __limit_goods[i]._node;
			
		//�ҵ���ǰ��Ч��group
		int active_group_id = 0;		
		if(node.group_active && __group_id != 0)
		{
			if(__group_id == node.entry[0].group_id || __group_id == node.entry[1].group_id || __group_id == node.entry[2].group_id || __group_id == node.entry[3].group_id)
				active_group_id = __group_id;	
		}
	
		if(!node.sale_time_active)		//ֻ�����õ�����ʱ��
		{
			//����Ч�ķ�Ĭ��group������	�����ͻ���
			ASSERT(node.group_active);
			if(active_group_id != 0)
			{
				for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
				{
					if(node.entry[j].cash_need <= 0)
						break;
					if(active_group_id == node.entry[j].group_id)	//�����������ҵ�һ��
					{
						CMD::Make<CMD::dividend_mall_item_price>::AddGoods(__h0, 
														index, 
														j, 
														node.goods_id, 
														node.entry[j].expire_type, 
														node.entry[j].expire_time, 
														node.entry[j].cash_need, 
														node.entry[j].status);
						__h0_count ++;
					}
				}
			}
		}
		else							//���ڷ����õ�����ʱ��
		{
			int available_permanent = -1;				//��Ч����������ʱ���slot������һ��
			int available_saletime = -1;				//��Ч�ķ���������ʱ���slot������һ��
			for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
			{
				if(node.entry[j].cash_need <= 0)
					break;
				if(node.entry[j].group_id == active_group_id && node.entry[j]._sale_time.CheckAvailable(__time))
				{
					if(node.entry[j]._sale_time.GetType() != netgame::mall::sale_time::TYPE_NOLIMIT)
					{
						available_saletime = j;
						break;		///ͬ����ֻ����һ������������ʱ�����㣬����ȷ������Ʒ��ǰ�ļ۸�
					}
					else
						available_permanent = j;	//���౻ִ��һ��	
				}			
			}
			if(available_saletime >= 0)		//ͬ����ֻ����һ������������ʱ�����㣬����ȷ������Ʒ��ǰ�ļ۸�
			{
				CMD::Make<CMD::dividend_mall_item_price>::AddGoods(__h0, 
														index, 
														available_saletime, 
														node.goods_id, 
														node.entry[available_saletime].expire_type, 
														node.entry[available_saletime].expire_time, 
														node.entry[available_saletime].cash_need, 
														node.entry[available_saletime].status);
				__h0_count ++;
			}
			else if(available_permanent >= 0 )	//ͬ��������������ʱ������
			{
				if(active_group_id != 0)	//ֻҪ��Ĭ��group�����ݲŻᷢ���ͻ���	
				{
					CMD::Make<CMD::dividend_mall_item_price>::AddGoods(__h0, 
																index, 
																available_permanent, 
																node.goods_id, 
																node.entry[available_permanent].expire_type, 
																node.entry[available_permanent].expire_time, 
																node.entry[available_permanent].cash_need, 
																node.entry[available_permanent].status);
					__h0_count ++;
				}
			}
			else		//��Ʒ�¼�,ֻ���ڿͻ��˵�gshop.data�д������õĹ���ʽʱ����Ҫ������Ʒ�¼�
			{
				if(active_group_id != 0)	//����0�������ǰ���Ѿ��жϿͻ��˿϶�û�����õĹ���ʽ
				{
					int m;
					for(int m=0; m<netgame::mall::MAX_ENTRY; m++)
					{
						if(node.entry[m].cash_need > 0 && node.entry[m].group_id == 0 
							&& node.entry[m]._sale_time.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
							break;
					}
					if(m < netgame::mall::MAX_ENTRY)
					{
						CMD::Make<CMD::dividend_mall_item_price>::AddGoods(__h0, 
																index, 
																0, 
																node.goods_id, 
																0, 
																0, 
																0, 
																0);
						__h0_count ++;
					}
				}
			}
		}//end of if(!sale_time_active)
	}
	
	packet_wrapper __h1(1024);
	CMD::Make<CMD::dividend_mall_item_price>::From(__h1, start_index, end_index, __h0_count);
	if(__h0_count > 0)
		__h1.push_back(__h0.data(), __h0.size());
	gplayer * pPlayer = (gplayer*)_parent;
	send_ls_msg(pPlayer,__h1);
	
	return true;
}

bool gplayer_imp::PlayerChooseMultiExp(int index)
{
	DATA_TYPE dt;
	MULTI_EXP_CONFIG * cfg = (MULTI_EXP_CONFIG *)world_manager::GetDataMan().get_data_ptr(701,ID_SPACE_CONFIG,dt);
	if(!cfg || dt != DT_MULTI_EXP_CONFIG) return false;

	if(index < 0 || (size_t)index >= sizeof(cfg->choice)/sizeof(cfg->choice[0])) return false;
	if(cfg->choice[index].rate < 1.f || cfg->choice[index].multi_time < 0 || cfg->choice[index].demulti_wait_time < 0 || cfg->choice[index].demulti_time < 0) return false;

	int stone_need = cfg->choice[index].item_count;
	int stone1_count, stone2_count;
	if(stone_need > 0)
	{
		stone1_count = GetItemCount(MULTI_EXP_STONE_ID);
		stone2_count = GetItemCount(MULTI_EXP_STONE_ID2);
		if(stone1_count + stone2_count < stone_need) return false;
	}
	
	if(!_multi_exp_ctrl.IncMultiExpTime(this,cfg->choice[index].rate,cfg->choice[index].multi_time*60,cfg->choice[index].demulti_wait_time*60,cfg->choice[index].demulti_time*60)) return false;
		
	if(stone_need > 0)
	{
		if(stone1_count >= stone_need)
			RemoveItems(MULTI_EXP_STONE_ID, stone_need, S2C::DROP_TYPE_USE, true);
		else
		{
			RemoveItems(MULTI_EXP_STONE_ID, stone1_count, S2C::DROP_TYPE_USE, true);
			RemoveItems(MULTI_EXP_STONE_ID2, stone_need-stone1_count, S2C::DROP_TYPE_USE, true);
		}
	}	
	return true;
}

bool 
gplayer_imp::PlayerGetFactionFortressInfo()
{
	if(!OI_IsMafiaMember()) return false;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return false;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return false;
	
	gplayer * pPlayer = (gplayer*)_parent;
	return pCtrl->GetInfo(pPlayer->ID.id, pPlayer->cs_index, pPlayer->cs_sid);
}

bool
gplayer_imp::TeamCongregateRequest(int reply_level_req, int reply_sec_level_req, int reply_reincarnation_times_req)
{
	if(IsCombatState())
	{
		_runner->error_message(S2C::ERR_INVALID_OPERATION_IN_COMBAT);
		return false;
	}
	if(((gplayer*)_parent)->IsInvisible()) 
	{
		_runner->error_message(S2C::ERR_OPERTION_DENYED_IN_INVISIBLE);
		return false;
	}
	if(!CheckCoolDown(COOLDOWN_INDEX_TEAM_CONGREGATE))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return false;
	}
	if(!IsInTeam()) return false;

	msg_congregate_req_t data;
	data.world_tag = world_manager::GetWorldTag();
	data.level_req = reply_level_req; 
	data.sec_level_req = reply_sec_level_req;
    data.reincarnation_times_req = reply_reincarnation_times_req;
	MSG msg;
	BuildMessage(msg, GM_MSG_CONGREGATE_REQUEST, XID(-1,-1), _parent->ID, _parent->pos, CONGREGATE_TYPE_TEAM, &data, sizeof(data));
	_team.SendGroupMessage(msg);
	SetCoolDown(COOLDOWN_INDEX_TEAM_CONGREGATE, TEAM_CONGREGATE_COOLDOWN_TIME);
	return true;
}

bool 
gplayer_imp::TeamMemberCongregateRequest(const XID& member)
{
	//ֻ���ڴ�����ʹ��
	if(world_manager::GetWorldTag() != 1)
	{
		_runner->error_message(S2C::ERR_OPERTION_DENYED_IN_CUR_SCENE);
		return false;
	}

	if(IsCombatState())
	{
		_runner->error_message(S2C::ERR_INVALID_OPERATION_IN_COMBAT);
		return false;
	}
	if(((gplayer*)_parent)->IsInvisible()) 
	{
		_runner->error_message(S2C::ERR_OPERTION_DENYED_IN_INVISIBLE);
		return false;
	}
	if(member == _parent->ID || !IsInTeam() || !IsMember(member)) return false;

	msg_congregate_req_t data;
	data.world_tag = world_manager::GetWorldTag();
	data.level_req = 0; 
	data.sec_level_req = 0;
    data.reincarnation_times_req = 0;
	SendTo<0>(GM_MSG_CONGREGATE_REQUEST,member,CONGREGATE_TYPE_TEAM_MEMBER,&data, sizeof(data));
	return true;
}

bool 
gplayer_imp::FactionCongregateRequest(int reply_level_req, int reply_sec_level_req, int reply_reincarnation_times_req)
{
	if(IsCombatState())
	{
		_runner->error_message(S2C::ERR_INVALID_OPERATION_IN_COMBAT);
		return false;
	}
	if(((gplayer*)_parent)->IsInvisible()) 
	{
		_runner->error_message(S2C::ERR_OPERTION_DENYED_IN_INVISIBLE);
		return false;
	}
	if(!CheckCoolDown(COOLDOWN_INDEX_FACTION_CONGREGATE))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return false;
	}
	//ֻ�а���/������/��������ʹ��
	char rank = OI_GetMafiaRank();
	if(rank != 2 && rank != 3 && rank != 4) return false;

	struct
	{
		world_pos wpos;
		int level_req;
		int sec_level_req;
        int reincarnation_times_req;
	}data;
	data.wpos.tag = world_manager::GetWorldTag();
	data.wpos.pos = _parent->pos;
	data.level_req = reply_level_req;
	data.sec_level_req = reply_sec_level_req;
    data.reincarnation_times_req = reply_reincarnation_times_req;
	bool b = GMSV::SendFactionCongregateRequest(OI_GetMafiaID(), _parent->ID.id, &data, sizeof(data));
	if(b) SetCoolDown(COOLDOWN_INDEX_FACTION_CONGREGATE, FACTION_CONGREGATE_COOLDOWN_TIME);
	return b;
}

namespace
{
	bool __check_sec_level(int req, int cur)
	{
		if(cur < req) return false;
		if(req >= 20) //���Ҫ�������Ϊ��ħ�������ƥ��
		{
			if(req/10 != cur/10) return false;
		}
		return true;
	}
}
void 
gplayer_imp::RecvCongregateRequest(char type, int sponsor, int world_tag, const A3DVECTOR& pos, int level_req, int sec_level_req, int reincarnation_times_req)
{
	if(_basic.level < level_req) return;
	if(!__check_sec_level(sec_level_req, _basic.sec_level)) return;
    if (GetReincarnationTimes() < (size_t)reincarnation_times_req) return;
	size_t i = 0;
	for(; i<_congregate_req_list.size(); i++)
	{
		if(_congregate_req_list[i].type == type) break;
	}
	if(i < _congregate_req_list.size())
	{
		_congregate_req_list[i].sponsor 	= sponsor;
		_congregate_req_list[i].timeout 	= g_timer.get_systime() + CONGREGATE_REQUEST_TIMEOUT;
		_congregate_req_list[i].world_tag 	= world_tag;
		_congregate_req_list[i].pos 		= pos;
	}
	else
	{
		congregate_req req;
		req.type 		= type;
		req.sponsor 	= sponsor;
		req.timeout 	= g_timer.get_systime() + CONGREGATE_REQUEST_TIMEOUT;
		req.world_tag 	= world_tag;
		req.pos			= pos;
		_congregate_req_list.push_back(req);
	}
	_runner->congregate_request(type, sponsor, CONGREGATE_REQUEST_TIMEOUT);
}

void 
gplayer_imp::PlayerCongregateReply(char type, char agree, int sponsor)
{
	int cur_time = g_timer.get_systime();
	abase::vector<congregate_req>::iterator it = _congregate_req_list.begin();
	for(; it!=_congregate_req_list.end(); ++it)
	{
		if(it->type == type && it->sponsor == sponsor && cur_time <= it->timeout) break;
	}
	if(it == _congregate_req_list.end()) return;
		
	if(agree)
	{
		LeaveAbnormalState();
		session_congregate * pSession = new session_congregate(this);
		pSession->SetDestination(type, it->world_tag, it->pos);
		if(AddSession(pSession)) StartSession();
	}
	else
	{
		SendTo<0>(GM_MSG_REJECT_CONGREGATE, XID(GM_TYPE_PLAYER,sponsor), type);
	}
	_congregate_req_list.erase(it);
}

void 
gplayer_imp::PlayerGetDpsDphRank(unsigned char rank_mask)
{
	if(!CheckCoolDown(COOLDOWN_INDEX_DPS_DPH_RANK))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return;
	}

	gplayer * pPlayer = (gplayer *)_parent;
	if(!world_manager::GetInstance()->DpsRankSendRank(pPlayer->cs_index, pPlayer->ID.id, pPlayer->cs_sid, rank_mask)) return;
	
	SetCoolDown(COOLDOWN_INDEX_DPS_DPH_RANK, DPS_DPH_RANK_COOLDOWN_TIME);
}

int 
gplayer_imp::PlayerJoinForce(int force_id)
{
	if(!world_manager::GetForceGlobalDataMan().IsDataReady()) return S2C::ERR_SERVICE_UNAVILABLE;
	if(!CheckCoolDown(COOLDOWN_INDEX_PLAYER_JOIN_FORCE)) return S2C::ERR_OBJECT_IS_COOLING;

	if(_player_force.GetForce() != 0) return -1;
	DATA_TYPE dt;
	FORCE_CONFIG * pCfg = (FORCE_CONFIG*) world_manager::GetDataMan().get_data_ptr(force_id, ID_SPACE_CONFIG,dt);
	if(dt != DT_FORCE_CONFIG || pCfg == NULL) return -1;

	if(pCfg->join_money_cost > 0)
	{
		if(GetMoney() < (size_t)pCfg->join_money_cost) return S2C::ERR_OUT_OF_FUND;
	}
	if(pCfg->join_item_cost > 0)
	{
		if(!IsItemExist(pCfg->join_item_cost)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	}
	
	_player_force.ChangeForce(force_id);
	GMSV::SendPlayerJoinOrLeaveForce(force_id, true);

	if(pCfg->join_money_cost > 0)
	{
		SpendMoney(pCfg->join_money_cost);
		_runner->spend_money(pCfg->join_money_cost);
	}
	if(pCfg->join_item_cost > 0)
	{
		TakeOutItem(pCfg->join_item_cost);
	}
	return 0;	
}

int 
gplayer_imp::PlayerLeaveForce()
{
	if(!world_manager::GetForceGlobalDataMan().IsDataReady()) return S2C::ERR_SERVICE_UNAVILABLE;
	if(!CheckCoolDown(COOLDOWN_INDEX_PLAYER_LEAVE_FORCE)) return S2C::ERR_OBJECT_IS_COOLING;

	int cur_force = _player_force.GetForce();
	if(cur_force == 0) return -1;
	DATA_TYPE dt;
	FORCE_CONFIG * pCfg = (FORCE_CONFIG*) world_manager::GetDataMan().get_data_ptr(cur_force, ID_SPACE_CONFIG,dt);
	if(dt != DT_FORCE_CONFIG || pCfg == NULL) return -1;

	if(pCfg->quit_repution_cost > 0 && pCfg->quit_repution_cost <= 100)
	{
		int repu = _player_force.GetReputation();
		int dec = (int)(repu * 0.01f * pCfg->quit_repution_cost);
		if(dec > 0)
			_player_force.DecReputation(dec);
	}
	if(pCfg->quit_contribution_cost > 0 && pCfg->quit_contribution_cost <= 100)
	{
		int contri = _player_force.GetContribution();
		int dec = (int)(contri * 0.01f * pCfg->quit_contribution_cost);
		if(dec > 0)
			_player_force.DecContribution(dec);
	}
	
	_player_force.ChangeForce(0);
	GMSV::SendPlayerJoinOrLeaveForce(cur_force, false);
	PlayerTaskInterface  task_if(this);
	OnTaskLeaveForce(&task_if);
	
	SetCoolDown(COOLDOWN_INDEX_PLAYER_JOIN_FORCE,PLAYER_JOIN_FORCE_COOLDOWN_TIME);
	SetCoolDown(COOLDOWN_INDEX_PLAYER_LEAVE_FORCE,PLAYER_LEAVE_FORCE_COOLDOWN_TIME);
	return 0;
}

void 
gplayer_imp::PlayerRechargeOnlineAward(int type, size_t count, int * list)
{
	if(type < 0 || type >= online_award::MAX_AWARD_TYPE) return;
	
	if(_basic.level < online_award::MIN_LEVEL_REQUIRED) return;
	
	DATA_TYPE dt;
	ONLINE_AWARDS_CONFIG * ess = (ONLINE_AWARDS_CONFIG *)world_manager::GetDataMan().get_data_ptr(ONLINE_AWARD_CONFIG_ID, ID_SPACE_CONFIG, dt);
	if(!ess || dt != DT_ONLINE_AWARDS_CONFIG) return;

	C2S::CMD::recharge_online_award::entry * ent = (C2S::CMD::recharge_online_award::entry *)list;
	abase::vector<char> flag_list;
	flag_list.insert(flag_list.begin(),_inventory.Size(),0);
	size_t total_count = 0;
	for(size_t i=0; i<count; i++)
	{
		int item_type = ent[i].type;
		if(!ent[i].count || !CheckItemExist(ent[i].index, item_type, ent[i].count)) return;
		if(flag_list[ent[i].index]++) return;
		
		bool find = false;
		for(size_t m=0; m<sizeof(ess->choice[type].ids)/sizeof(ess->choice[type].ids[0]); m++)
		{
			if(ess->choice[type].ids[m] <= 0) break;
			if(item_type == (int)ess->choice[type].ids[m])
			{
				find = true;
				break;
			}
		}
		if(!find) return;
		total_count += ent[i].count;
	}

	float inc_time = (float)total_count * ess->choice[type].time;
	if(inc_time <= 0.f || inc_time > 2e9) return;
	if(!_online_award.IncAwardTime(this, type, total_count * ess->choice[type].time)) return;

	for(size_t i=0; i<count; i++)
	{
		item& it = _inventory[ent[i].index];
		UpdateMallConsumptionDestroying(it.type, it.proc_type, ent[i].count);
		
		_inventory.DecAmount(ent[i].index, ent[i].count);
		_runner->player_drop_item(gplayer_imp::IL_INVENTORY,ent[i].index,ent[i].type,ent[i].count,S2C::DROP_TYPE_USE);
	}
}

bool 
gplayer_imp::PlayerGetCountryBattlePersonalScore()
{
	if(!GetCountryId()) return false;

	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return false;

	 int combat_time, attend_time, kill_count, death_count, country_kill_count, country_death_count;
	 gplayer * pPlayer = GetParent();
	if(pCtrl->GetPersonalScore(pPlayer->IsBattleOffense(), pPlayer->ID.id, combat_time, attend_time, kill_count, death_count, country_kill_count, country_death_count))
	{
		_runner->countrybattle_personal_score(combat_time,attend_time,kill_count,death_count,country_kill_count,country_death_count);	
		return true;
	}
	return false;
}

bool 
gplayer_imp::PlayerGetCountryBattleStrongholdState()
{
	if(!GetCountryId()) return false;

	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return false;
	
	gplayer * pPlayer = (gplayer*)_parent;
	pCtrl->GetStongholdState(pPlayer->ID.id, pPlayer->cs_index, pPlayer->cs_sid);	
	return true;
}

bool 
gplayer_imp::PlayerGetCountryBattleLiveShow()
{
	if(!GetCountryId()) return false;

	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return false;	
	if(!CheckCoolDown(COOLDOWN_INDEX_COUNTRYBATTLE_LIVESHOW)) return false;

	gplayer * pPlayer = (gplayer*)_parent;
	bool ret = pCtrl->GetLiveShowResult(pPlayer->ID.id, pPlayer->cs_index, pPlayer->cs_sid, _plane);
	SetCoolDown(COOLDOWN_INDEX_COUNTRYBATTLE_LIVESHOW, COUNTRYBATTLE_LIVESHOW_COOLDOWN_TIME);
	return ret;
}

void 
gplayer_imp::PlayerLeaveCountryBattle()
{
	if(!world_manager::GetInstance()->IsCountryBattleWorld()) return;
	
	_filters.ModifyFilter(FILTER_CHECK_INSTANCE_KEY,FMID_CLEAR_AECB,NULL,0);
}

int 
gplayer_imp::PlayerTryChangeDS(int flag)
{
	if(flag == GMSV::CHDS_FLAG_DS_TO_CENTRALDS)
	{
		if(GetHistoricalMaxLevel() < CHANGEDS_LEVEL_REQUIRED) return S2C::ERR_LEVEL_NOT_MATCH;
		if(_basic.sec_level < CHANGEDS_SEC_LEVEL_REQUIRED) return S2C::ERR_SEC_LEVEL_NOT_MATCH;
	}
	else if(flag == GMSV::CHDS_FLAG_CENTRALDS_TO_DS)
	{
	}
	else
	{
		ASSERT(false);
		return -1;
	}
	
	if(!CheckCoolDown(COOLDOWN_INDEX_CHANGEDS)) return S2C::ERR_OBJECT_IS_COOLING;
	SetCoolDown(COOLDOWN_INDEX_CHANGEDS,CHANGEDS_COOLDOWN_TIME);

	GMSV::SendTryChangeDS(_parent->ID.id, flag);
	return 0;
}

void 
gplayer_imp::PlayerChangeDSLogout(int flag)
{
	if((_player_state != PLAYER_STATE_NORMAL
				&& _player_state != PLAYER_SIT_DOWN
				&& _player_state != PLAYER_STATE_BIND) 
				|| IsCombatState())
	{
		_runner->error_message(S2C::ERR_CANNOT_CHANGEDS);
		return;
	}
	LeaveAbnormalState();
	GLog::log(GLOG_INFO,"�û�%dִ�п�������߼�%d",_parent->ID.id, flag);
	_offline_type = PLAYER_OFF_CHANGEDS;
	_team.PlayerLogout();
	Logout(flag);
}

void 
gplayer_imp::PlayerExchangeWanmeiYinpiao(bool is_sell, size_t count)
{
	if(_player_state == PLAYER_STATE_MARKET)
	{
		if(_stall_info.stallcard_id == -1)
		{
			//û�а�̯ƾ֤��ֹ�һ�
			return;
		}
		ASSERT(_stall_obj);
		if(is_sell && _stall_obj->IsGoodsExist(WANMEI_YINPIAO_ID))
		{
			//�����б��д�����Ʊ��ֹ�һ�
			return;
		}
	}
	else
	{
		//�ǰ�̯״̬��ֹ�һ�
		return;
	}

	if(_lock_inventory)
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return;
	}
	if(world_manager::GetDataMan().get_item_sell_price(WANMEI_YINPIAO_ID) != WANMEI_YINPIAO_PRICE)
	{
		return;
	}
	
	if(is_sell)
	{
		if(!count || !CheckItemExist(WANMEI_YINPIAO_ID, count)) return;
		if((float)count * WANMEI_YINPIAO_PRICE > 2e9) return;
		size_t incmoney = count * WANMEI_YINPIAO_PRICE;
		if(!CheckIncMoney(incmoney))
		{
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			return;
		}
		//
		RemoveItems(WANMEI_YINPIAO_ID, count, S2C::DROP_TYPE_TAKEOUT, false);
		GainMoney(incmoney);
		_runner->get_player_money(GetMoney(),_money_capacity);
		GLog::log(GLOG_INFO,"�û�%d��%d����Ʊ�һ�Ϊ���%d", _parent->ID.id, count, incmoney);
	}
	else
	{
		if(!count || count > (size_t)world_manager::GetDataMan().get_item_pile_limit(WANMEI_YINPIAO_ID)) return;
		if((float)count * WANMEI_YINPIAO_PRICE > 2e9) return;
		size_t decmoney = count * WANMEI_YINPIAO_PRICE;
		if(GetMoney() < decmoney)
		{
			_runner->error_message(S2C::ERR_OUT_OF_FUND);
			return;
		}
		if(!_inventory.HasSlot(WANMEI_YINPIAO_ID,count))
		{
			_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
			return;
		}
		//
		SpendMoney(decmoney);
		_runner->spend_money(decmoney);

		element_data::item_tag_t tag = {element_data::IMT_NULL,0};
		item_data * data = world_manager::GetDataMan().generate_item_from_player(WANMEI_YINPIAO_ID, &tag, sizeof(tag));	
		if(data == NULL)
		{
			ASSERT(false);
			return;	
		}
		data->count = count;
		int rst = _inventory.Push(*data);
		if(rst < 0 || data->count)
		{
			ASSERT(false);
			FreeItem(data);
			return;
		}
		_runner->obtain_item(WANMEI_YINPIAO_ID,0,count,_inventory[rst].count,IL_INVENTORY,rst);
		FreeItem(data);
		GLog::log(GLOG_INFO,"�û�%d���ѽ��%d�һ�Ϊ%d����Ʊ", _parent->ID.id, decmoney, count);
	}
}

int 
gplayer_imp::PlayerDecomposeFashionItem(size_t inv_index, int fashion_type, size_t fee, int production_type)
{
	if(inv_index >= _inventory.Size()) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	item & it = _inventory[inv_index];
	if(it.type <= 0 || it.type != fashion_type) return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	DATA_TYPE dt; 
	FASHION_ESSENCE * ess = (FASHION_ESSENCE *)world_manager::GetDataMan().get_data_ptr(fashion_type, ID_SPACE_ESSENCE, dt);
	if(ess == NULL || dt != DT_FASHION_ESSENCE) return S2C::ERR_DECOMPOSE_FAILED;
	//���ʱװ�õ���Ⱦɫ����������Ⱦɫʱ��Ҫ������
	if(ess->require_dye_count <= 0) return S2C::ERR_DECOMPOSE_FAILED;
	//���˵�ʱ�޵�ʱװ���ܲ��
	if((it.proc_type & item::ITEM_PROC_TYPE_BIND) || it.expire_date > 0) return S2C::ERR_DECOMPOSE_FAILED;
	
	if(GetMoney() < fee) return S2C::ERR_OUT_OF_FUND;

	element_data::item_tag_t tag = {element_data::IMT_NULL, 0};
	item_data * data = world_manager::GetDataMan().generate_item_from_player(production_type, &tag, sizeof(tag));
	if(!data) return S2C::ERR_DECOMPOSE_FAILED;
		
	GLog::log(GLOG_INFO,"�û�%d�ֽ�ʱװ%d�õ�%d��%d",_parent->ID.id,fashion_type,ess->require_dye_count,production_type);
	SpendMoney(fee);
	_runner->spend_money(fee);
	
	UpdateMallConsumptionDestroying(fashion_type, it.proc_type, 1);
	_inventory.DecAmount(inv_index, 1);
	_runner->player_drop_item(IL_INVENTORY,inv_index,fashion_type,1,S2C::DROP_TYPE_DECOMPOSE);

	size_t count = ess->require_dye_count;
	bool inv_isfull = false;
	while(count)
	{
		data->count = count;
		if(data->count > data->pile_limit) data->count = data->pile_limit;
		count -= data->count;
		if(!inv_isfull)
		{
			int ocount = data->count;
			int rst;
			if((rst = _inventory.Push(*data)) >= 0)
			{
				_runner->obtain_item(data->type,data->expire_date,ocount-data->count,_inventory[rst].count,0,rst);
			}
		}
		if(data->count)
		{
			DropItemFromData(_plane,_parent->pos,*data,_parent->ID,0,0,_parent->ID.id);
			inv_isfull = true;
		}
	}

	FreeItem(data);
	return 0;
}

void
gplayer_imp::TaskSendMessage(int task_id, int channel, int param)
{
	struct
	{
		int self_id;
		int task_id;
		int channel;
		int param; 
		int world_tag;
		char name[MAX_USERNAME_LENGTH];
	}data;          
	memset(&data, 0,sizeof(data)); 
	data.self_id = _parent->ID.id;
	data.task_id = task_id;
	data.channel = channel;
	data.param = param;
	data.world_tag  = world_manager::GetWorldTag();
	size_t  len = _username_len;
	if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
	memcpy(data.name, _username,len);

	switch(channel)
	{
		case GMSV::CHAT_CHANNEL_LOCAL:
		case GMSV::CHAT_CHANNEL_TRADE:
			{
				slice * pPiece = GetParent()->pPiece;
				if(pPiece && _plane)
				{
					AutoBroadcastChatMsg(_plane,pPiece,&data,sizeof(data), GMSV::CHAT_CHANNEL_SYSTEM, _chat_emote ,0,0,TASK_CHAT_MESSAGE_ID,0);
				}
				return ;
			}

		case GMSV::CHAT_CHANNEL_FARCRY:
		case GMSV::CHAT_CHANNEL_BROADCAST:
			broadcast_chat_msg(TASK_CHAT_MESSAGE_ID,&data,sizeof(data),GMSV::CHAT_CHANNEL_SYSTEM, _chat_emote,0,0);
			return;

		case GMSV::CHAT_CHANNEL_TEAM:
			SendTeamChat(GMSV::CHAT_CHANNEL_SYSTEM, &data, sizeof(data), 0, 0, TASK_CHAT_MESSAGE_ID);
			return;

		case GMSV::CHAT_CHANNEL_WHISPER:
		case GMSV::CHAT_CHANNEL_FACTION:
			return ;
	}

	return ;
}

void gplayer_imp::ItemMakeSlot(size_t index, int id)
{
	if(id <= 0) return;

	//�����Ʒ�Ƿ�����Լ��Ƿ�Ϸ�
	if(index >= _inventory.Size() ) return;
	item & it = _inventory[index];
	if(it.type <= 0 || it.type != id || it.body == NULL) return;

	int new_slot;
	if(int err_code = it.body->MakeSlot(this, new_slot))
	{
		//�޷���׻��ߴ��ʧ��
		_runner->error_message(err_code);
		return ;
	}
	else
	{
		//����־
		GLog::log(GLOG_INFO,"���%d����Ʒ%d�����˴�ײ�������������%d����", _parent->ID.id, id, new_slot);
		_runner->error_message(S2C::ERR_MAKE_SLOT_SUCCESS);
	}
	
	//֪ͨ�ͻ������ݸ���
	PlayerGetItemInfo(IL_INVENTORY,index);
	return ;
}

void 
gplayer_imp::RepairDamagedItem(unsigned char inv_idx)
{
	if(inv_idx >= _inventory.Size()) return;
	item & it = _inventory[inv_idx];
	if(it.type <= 0 || !(it.proc_type & item::ITEM_PROC_TYPE_DAMAGED)) return;

	unsigned int damaged_drop;
	int damaged_drop_count;
	if( (damaged_drop_count = world_manager::GetDataMan().get_item_damaged_drop(it.type,damaged_drop)) <= 0) return;
	DATA_TYPE dt;
	const void * ess = world_manager::GetDataMan().get_data_ptr(damaged_drop,ID_SPACE_ESSENCE,dt);
	if(ess == NULL || dt == DT_INVALID) return;

	damaged_drop_count = (int)ceil(damaged_drop_count*1.2);
	if(!CheckItemExist(damaged_drop, damaged_drop_count)) return;
	
	RemoveItems(damaged_drop, damaged_drop_count, S2C::DROP_TYPE_USE, true);
	it.proc_type &= ~item::ITEM_PROC_TYPE_DAMAGED;
	PlayerGetItemInfo(IL_INVENTORY,inv_idx);
}

void 
gplayer_imp::GodEvilConvert(unsigned char mode)
{
    enum
    {
        MODE_0 = 0,
        MODE_1,
        MODE_MAX,
    };
    static const int ticket_id_list[MODE_MAX] = { GOD_EVIL_CONVERT_TICKET_ID, GOD_EVIL_CONVERT_TICKET_ID2 };
    static const int config_id_list[MODE_MAX][USER_CLASS_COUNT] =
    { 
        { 1774, 1775, 1781, 1777, 1776, 1780, 1778, 1779, 1782, 1783, 1784, 1785 },  
        { 1786, 1787, 1793, 1789, 1788, 1792, 1790, 1791, 1794, 1795, 1796, 1797 }  
    };

	if(_basic.sec_level != 22 && _basic.sec_level != 32) return;
	if(mode >= MODE_MAX) return;
    int ticket_id = ticket_id_list[mode];
	if(!CheckItemExist(ticket_id, 1)) return;
    
    int cls = GetPlayerClass();
    if((cls < USER_CLASS_SWORDSMAN) && (cls >= USER_CLASS_COUNT)) return;
    int config_id = config_id_list[mode][cls];

	DATA_TYPE convert_dt;
	GOD_EVIL_CONVERT_CONFIG * convert_ess = (GOD_EVIL_CONVERT_CONFIG *)world_manager::GetDataMan().get_data_ptr(config_id, ID_SPACE_CONFIG, convert_dt);	
	if(!convert_ess || convert_dt != DT_GOD_EVIL_CONVERT_CONFIG) return;	
	
	RemoveItems(ticket_id, 1, S2C::DROP_TYPE_USE, true);
	std::map<int,int> convert_table;
	for(size_t i=0; i<sizeof(convert_ess->skill_map)/sizeof(convert_ess->skill_map[0]); i++)
	{
		int skill1 = convert_ess->skill_map[i][0], skill2 = convert_ess->skill_map[i][1];
		if(skill1 <= 0 || skill2 <= 0)
		{
			if(skill1 > 0 || skill2 > 0)
				GLog::log(GLOG_ERR, "��ħת�����м��ܲ�ͬʱΪ��skill_map[%d]", i);
			continue;	
		}
		if(_skill.GetType(skill1) == -1 || _skill.GetType(skill2) == -1)
		{
			GLog::log(GLOG_ERR, "��ħת�����м��ܲ�����skill_map[%d]", i);
			continue;	
		}
		if(convert_table.find(skill1) != convert_table.end() || convert_table.find(skill2) != convert_table.end())
		{
			GLog::log(GLOG_ERR, "��ħת�����м������ظ�skill_map[%d]", i);
			continue;
		}
		convert_table.insert(std::make_pair(skill1,skill2));
		convert_table.insert(std::make_pair(skill2,skill1));
	}
	_skill.GodEvilConvert(convert_table, object_interface(this), _cur_item.weapon_class, GetForm());
	_runner->get_skill_data();
	if(_basic.sec_level == 22) 
		SetSecLevel(32);
	else 
		SetSecLevel(22);
}

int 
gplayer_imp::WeddingBook(int start_time, int end_time, int scene, int bookcard_index)
{
	//���봦�ڶ�����ֻ�жӳ��ܷ���������
	if(!IsInTeam() || !IsTeamLeader()) return S2C::ERR_WEDDING_CANNOT_BOOK; 
	if(GetTeamMemberNum() != 2) return S2C::ERR_WEDDING_CANNOT_BOOK;
	//�������������1������ȴ
	if(!CheckAndSetCoolDown(COOLDOWN_INDEX_TEAM_RELATION, 1000)) return S2C::ERR_WEDDING_CANNOT_BOOK;

	//�Ƚ��м򵥵ļ��
	if(!IsMarried()) return S2C::ERR_WEDDING_CANNOT_BOOK;

	if(_inventory.GetEmptySlotCount() < 2) return S2C::ERR_INVENTORY_IS_FULL;	
	if(!CheckItemExist(WEDDING_BOOK_TICKET_ID, 1)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	
	int year = 0, month = 0, day = 0;
	if(bookcard_index >= 0)
	{
		if((size_t)bookcard_index >= _inventory.Size()) return S2C::ERR_ITEM_NOT_IN_INVENTORY;	
		item & it = _inventory[bookcard_index];
		if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_WEDDING_BOOKCARD) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
		if(!it.GetBookCardData(year,month,day)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	}
	if(!world_manager::GetInstance()->WeddingCheckBook(start_time,end_time,scene,year,month,day)) return S2C::ERR_WEDDING_CANNOT_BOOK;
	
	if(scene < 0 || scene >= WEDDING_SCENE_COUNT) return S2C::ERR_WEDDING_CANNOT_BOOK;
	
	XID list[TEAM_MEMBER_CAPACITY];
	int count = GetMemberList(list);
	ONET::Thread::Pool::AddTask( new WeddingBookJob(this, list, count, start_time, end_time, scene, bookcard_index));
	return 0;
}

int 
gplayer_imp::WeddingCancelBook(int _start_time, int _end_time, int _scene)
{
	//���봦�ڶ�����ֻ�жӳ��ܷ���������
	if(!IsInTeam() || !IsTeamLeader()) return S2C::ERR_WEDDING_CANNOT_CANCELBOOK; 
	if(GetTeamMemberNum() != 2) return S2C::ERR_WEDDING_CANNOT_CANCELBOOK;
	//�������������1������ȴ
	if(!CheckAndSetCoolDown(COOLDOWN_INDEX_TEAM_RELATION, 1000)) return S2C::ERR_WEDDING_CANNOT_CANCELBOOK;

	if(_inventory.GetEmptySlotCount() < 1) return S2C::ERR_INVENTORY_IS_FULL;	
	int start_time, end_time, groom, bride, scene, invitee;
	int rst = 0;
	while( (rst = _inventory.Find(rst,WEDDING_INVITECARD_ID1)) >= 0)
	{
		item & it = _inventory[rst];
		if(it.GetInviteCardData(start_time, end_time, groom, bride, scene, invitee))
		{
			if(start_time == _start_time
					&& end_time == _end_time
					&& scene == _scene
					&& (groom == _parent->ID.id || bride == _parent->ID.id)
					&& invitee == _parent->ID.id)
				break;
		}
		rst++;
	}
	if(rst < 0) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(GetMoney() < WEDDING_CANCELBOOK_FEE) return S2C::ERR_OUT_OF_FUND; 
	if(!world_manager::GetInstance()->WeddingCheckCancelBook(start_time,end_time,groom,bride,scene)) return S2C::ERR_WEDDING_CANNOT_CANCELBOOK;

	XID list[TEAM_MEMBER_CAPACITY];
	int count = GetMemberList(list);
	ONET::Thread::Pool::AddTask( new WeddingCancelBookJob(this, list, count, start_time, end_time, scene));
	return 0;
}

int 
gplayer_imp::WeddingInvite(int invitecard_index, int invitee)
{
	if(invitecard_index < 0 || (size_t)invitecard_index >= _inventory.Size()) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	item & it = _inventory[invitecard_index];
	if(it.type <= 0 || it.type != WEDDING_INVITECARD_ID1 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_WEDDING_INVITECARD) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	int start_time, end_time, groom, bride, scene, self;
	if(!it.GetInviteCardData(start_time, end_time, groom, bride, scene, self)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(_parent->ID.id != self) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(invitee < 0 || invitee == groom || invitee == bride) return S2C::ERR_PLAYER_NOT_EXIST;
	
	if(!CheckItemExist(WEDDING_INVITE_TICKET_ID,1)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(_inventory.GetEmptySlotCount() < 1) return S2C::ERR_INVENTORY_IS_FULL;	
	//���ɿյ����		
	element_data::item_tag_t tag = {element_data::IMT_NULL, 0};
	item_data * invitecard_data = world_manager::GetDataMan().generate_item_from_player(WEDDING_INVITECARD_ID2, &tag, sizeof(tag)); 
	if(!invitecard_data)
	{
		GLog::log(GLOG_ERR,"���ɱ���ʹ�õ����ʧ��");
		return S2C::ERR_FATAL_ERR;
	}
	//����ڻ������24Сʱ��ʧ
	invitecard_data->expire_date = end_time + 86400;
	int rst = _inventory.PushInEmpty(0, *invitecard_data, 1);
	if(rst >= 0)
	{
		item & it2 = _inventory[rst];
		it2.SetInviteCardData(start_time, end_time, groom, bride, scene, invitee);
		_runner->obtain_item(invitecard_data->type, invitecard_data->expire_date, 1, it2.count, IL_INVENTORY, rst);	
	}
	else
	{
		ASSERT(false);
		FreeItem(invitecard_data);
		return S2C::ERR_FATAL_ERR;
	}
	RemoveItems(WEDDING_INVITE_TICKET_ID, 1, S2C::DROP_TYPE_USE, true);
	FreeItem(invitecard_data);
	return 0;
}

int	
gplayer_imp::GetKnockBackPos(const A3DVECTOR& attacker_pos, float back_distance, A3DVECTOR& back_pos) 
{
	//ʵ�ַ���:ֱ�ߺ��ˣ����ݵ��Ρ���ײ�������˵�
	//����ֵ:0 ���˳ɹ� 1 ����һ���� -1 ��ȫû����
	//������˷���
	A3DVECTOR back_dir = _parent->pos;
	back_dir -= attacker_pos;
	float sq = back_dir.squared_magnitude();
	if(sq <= 1e-6)
	{
		//��������²�����
		return -1;
	}
	back_dir *= 1/sqrt(sq);
	//������˲���
	static const float step_distance = 0.5f;		//������˵�ʱ���õĲ����
	int step_count = int(back_distance/step_distance+0.5f) + 1; //��Ϊʹ��ǰһ���ɹ����˵Ľ��������ʱ�����һ��
	if(step_count < 2) return -1;	//���˾���̫С 
	
	static const float threshold = 0.6f;			//�������˵�ʱ�ĵ�����ֵ
	const A3DVECTOR & ext = aExts2[(IsPlayerFemale() ? 2*GetPlayerClass()+1 : 2*GetPlayerClass())];
	
	A3DVECTOR curpos = _parent->pos;
	A3DVECTOR prevpos = curpos, nextpos;
	A3DVECTOR step_offset;
	int i = 0;
	for( ; i<step_count; i++)
	{
		step_offset = back_dir;
		step_offset *= step_distance;

		nextpos = curpos;
		nextpos += step_offset;
		//���߶�
		float terrain_height = _plane->GetHeightAt(nextpos.x,nextpos.z);
		if(nextpos.y < terrain_height)
		{
			//��������
			float terrain_height0 = _plane->GetHeightAt(curpos.x,curpos.z);
			A3DVECTOR ground_dir(nextpos.x,terrain_height,nextpos.z);
			ground_dir -= A3DVECTOR(curpos.x,terrain_height0,curpos.z);
			if(step_offset.dot_product(ground_dir) <= 0.707*step_distance*sqrt(ground_dir.squared_magnitude()))
			{
				//���˷��������н�>45�ȣ�ֹͣ����
				break;
			}
			
			if(terrain_height > nextpos.y + step_distance*threshold)
			{
				//�Ժ��˵��yֵ�ĵ���Ҫ��������
				break;
			}
			
			//char buf[100] = {0};
			//sprintf(buf,"Height : up adjust %f", terrain_height - nextpos.y);
			//Say(buf);
			
			//�������˵���������
			step_offset.y += terrain_height - nextpos.y ;
			nextpos.y = terrain_height;
		}
		//�����ײ
		trace_manager2 & man = *(_plane->GetTraceMan());
		if(man.Valid())
		{
			bool is_solid;
			float ratio;
			bool bRst = man.AABBTrace(curpos, step_offset, ext, is_solid,ratio,&_plane->w_collision_flags);
			if(bRst)
			{
				if(is_solid) break;//��̫����
				//����ײ����ߵ�ǰ��߶��ڲ���һ�Σ�������ײб�µ����	
				A3DVECTOR tmp = curpos;
				tmp.y += step_distance*threshold;
				bool bRst1 = man.AABBTrace(tmp, step_offset, ext, is_solid,ratio,&_plane->w_collision_flags);
				if(bRst1)
				{
					//����Ҳ����ײ������ֹͣ
					break;
				}
				else
				{
					//����û����ײ,������֮��Ѱ�Һ��˵�	
					tmp += step_offset;
					bool bRst2 = man.AABBTrace(tmp, A3DVECTOR(0.f, -step_distance*threshold, 0.f), ext, is_solid,ratio,&_plane->w_collision_flags);
					if(bRst2)
					{
						if(is_solid) break;//��̫����
						step_offset.y += step_distance*threshold*(1.f-ratio);
						nextpos.y += step_distance*threshold*(1.f-ratio);					
						
						//char buf[100] = {0};
						//sprintf(buf,"AABBTrace : up adjust %f, bRst2 %d solid %d ratio %f",
						//		step_distance*threshold*(1.f-ratio),bRst2,is_solid,ratio);
						//Say(buf);
					}
					else
					{
						;//�����������Ϊ�˺��˵���Ч
					}
				}
			}
		}
	
		prevpos = curpos;
		curpos = nextpos;	
		
		//DropMoneyItem(_plane,curpos,i+1,XID(0,0),0,0,_parent->ID.id);
	}
	//��Ϊʹ��ǰһ���ɹ����˵Ľ�����ɹ������������ϲ���ɹ�
	if(i < 2) return -1;
	//ʹ��ǰһ���ɹ����˵Ľ�����������￨�뽨���ڸ���
	back_pos = prevpos;	
	return (i==step_count ? 0 : 1);	
}

int
gplayer_imp::FactionFortressLevelUp()
{
	if(!OI_IsMafiaMaster()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	if(!pCtrl->LevelUp()) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	PlayerGetFactionFortressInfo();
	return 0;
}

int
gplayer_imp::FactionFortressSetTechPoint(size_t tech_index)
{
	if(!OI_IsMafiaMaster()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	if(!pCtrl->SetTechPoint(tech_index)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	PlayerGetFactionFortressInfo();
	return 0;
}

int
gplayer_imp::FactionFortressResetTechPoint(size_t tech_index, size_t inv_index, int id)
{
	if(!OI_IsMafiaMaster()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;

	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;

	if(FACTION_FORTRESS_RESET_TECH_ITEM_ID != id || !CheckItemExist(inv_index,id,1)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	if(!pCtrl->ResetTechPoint(_plane, tech_index)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	_inventory.DecAmount(inv_index, 1);
	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,inv_index,id,1,S2C::DROP_TYPE_USE);
	PlayerGetFactionFortressInfo();
	return 0;
}

int
gplayer_imp::FactionFortressConstruct(int id, int accelerate)
{
	if(!OI_IsMafiaMaster()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	DATA_TYPE dt;
	FACTION_BUILDING_ESSENCE * ess = (FACTION_BUILDING_ESSENCE *)world_manager::GetDataMan().get_data_ptr(id,ID_SPACE_ESSENCE,dt);
	if(!ess || dt != DT_FACTION_BUILDING_ESSENCE) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	if(GetMoney() < (size_t)ess->money) return S2C::ERR_OUT_OF_FUND;
	if(!pCtrl->Construct(_plane,id,accelerate)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	SpendMoney((size_t)ess->money);
	_runner->spend_money((size_t)ess->money);
	PlayerGetFactionFortressInfo();
	return 0;
}

int
gplayer_imp::FactionFortressHandInMaterial(size_t inv_index, int id, size_t count)
{
	if(!OI_IsMafiaMember()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;

	if(!count || !CheckItemExist(inv_index,id,count)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(!pCtrl->HandInMaterial(id,count)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	_inventory.DecAmount(inv_index, count);
	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,inv_index,id,count,S2C::DROP_TYPE_USE);
	PlayerGetFactionFortressInfo();
	
	if(count >= 10)
	{
		struct
		{
			int material_id;
			size_t material_count;
			char name[MAX_USERNAME_LENGTH];
		}data;
		memset(&data,0,sizeof(data));
		data.material_id = id;
		data.material_count = count;
		size_t  len = _username_len;
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(data.name, _username,len);
		GMSV::FactionBroadcastMsg(OI_GetMafiaID(),GMSV::CMSG_FF_HANDINMATERIAL,&(data),sizeof(data));
	}
	return 0;
}

int
gplayer_imp::FactionFortressHandInContrib(int contrib)
{
	if(!OI_IsMafiaMember()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(contrib <= 0 || GetFactionExpContrib() < contrib) return S2C::ERR_NOT_ENOUGH_FACTION_CONTRIB;
	if(!pCtrl->HandInContrib(contrib)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	DecFactionContrib(0,contrib);
	PlayerGetFactionFortressInfo();
	
	if(contrib >= 5000)
	{
		struct
		{
			int contrib;
			char name[MAX_USERNAME_LENGTH];
		}data;
		memset(&data,0,sizeof(data));
		data.contrib = contrib;
		size_t  len = _username_len;
		if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
		memcpy(data.name, _username,len);
		GMSV::FactionBroadcastMsg(OI_GetMafiaID(),GMSV::CMSG_FF_HANDINCONTRIB,&(data),sizeof(data));
	}
	return 0;
}

int 
gplayer_imp::FactionFortressMaterialExchange(size_t src_index,size_t dst_index,int material)
{
	if(!OI_IsMafiaMaster()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	if(!pCtrl->MaterialExchange(src_index,dst_index,material)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	PlayerGetFactionFortressInfo();
	return 0;
}

int 
gplayer_imp::FactionFortressDismantle(int id)
{
	if(!OI_IsMafiaMaster()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return S2C::ERR_FACTION_FORTRESS_OP_DENYED;
	if(!pCtrl->Dismantle(_plane,id)) return S2C::ERR_FACTION_FORTRESS_OP_FAILED;
	PlayerGetFactionFortressInfo();
	return 0;
}

void 
gplayer_imp::FindSpecItem(unsigned char where, int type, archive& ar)
{
	item_list * plist = NULL;
	if(where <= IL_INVENTORY_END) plist = &GetInventory(where);
	else if(where >= IL_TRASH_BOX_BEGIN && where <= IL_TRASH_BOX_END) plist = &GetTrashInventory(where);
	else return;
	
	int start = 0;
	while( (start = (*plist).Find(start, type)) >= 0)
	{
		ar << where << (unsigned char)start << (*plist)[start].count;
		start ++;	
	}
}

int
gplayer_imp::RemoveSpecItem(unsigned char where, unsigned char index, size_t count, int type)
{
	item_list * plist = NULL;
	if(where <= IL_INVENTORY_END) plist = &GetInventory(where);
	else if(where >= IL_TRASH_BOX_BEGIN && where <= IL_TRASH_BOX_END) plist = &GetTrashInventory(where);
	else return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	if(index >= (*plist).Size()) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	item & it = (*plist)[index];
	if(it.type != type || it.count < count) return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	(*plist).DecAmount(index, count);
	_runner->player_drop_item(where,index,type,count,S2C::DROP_TYPE_GM);
	if(where == IL_EQUIPMENT)
	{
		//����װ���ı��־
		IncEquipChangeFlag();
		//����ˢ��װ��
		RefreshEquipment();
		//����������Ϣ
		CalcEquipmentInfo();
		_runner->equipment_info_changed(0,1ULL<<index,0,0); //�˺���ʹ����CalcEquipmentInfo�Ľ��
	}
	
	return S2C::ERR_SUCCESS;
}

bool
gplayer_imp::TryTransmitSkillAttack(const MSG & msg)
{
	if(!_skill_attack_transmit_target.IsActive()) return false;
	if(_skill_attack_transmit_target == msg.source) return false;
	world::object_info info;
	if(!_plane->QueryObject(_skill_attack_transmit_target,info)) return false;
	if(info.state & world::QUERY_OBJECT_STATE_ZOMBIE) return false;
	
	if(msg.message == GM_MSG_ATTACK)
	{
		const attack_msg & ack_msg = *(const attack_msg*)msg.content;
		if(ack_msg.skill_id == 0) return false;
		//���ӹ�����Χ
		attack_msg new_ack_msg(ack_msg);
		new_ack_msg.attack_range += sqrt(info.pos.squared_distance(_parent->pos));
		MSG newmsg(msg);
		newmsg.target = _skill_attack_transmit_target;
		newmsg.content_length = sizeof(new_ack_msg);
		newmsg.content = &new_ack_msg;
		_plane->PostLazyMessage(newmsg);
		return true;
	}
	else if(msg.message == GM_MSG_ENCHANT)
	{
		const enchant_msg & ech_msg = *(const enchant_msg*)msg.content;
		if(ech_msg.helpful) return false;
		MSG newmsg(msg);
		newmsg.target = _skill_attack_transmit_target;
		_plane->PostLazyMessage(newmsg);
		return true;
	}
	else
	{
		ASSERT(false);
		return false;
	}
}

bool 
gplayer_imp::IncForceReputation(int value)
{
	if(_player_force.GetForce() == 0) return false;
	DATA_TYPE dt;
	FORCE_CONFIG * pCfg = (FORCE_CONFIG*) world_manager::GetDataMan().get_data_ptr(_player_force.GetForce(), ID_SPACE_CONFIG,dt);
	if(dt != DT_FORCE_CONFIG || pCfg == NULL) return false;

	int repu = _player_force.GetReputation();
	if(value > pCfg->reputation_max - repu) value = pCfg->reputation_max - repu;
	if(value <= 0) return false;

	//�������ߵ�������
	if(_force_ticket_info.repu_inc_ratio > 0 && 
			(_force_ticket_info.require_force == 0 || _force_ticket_info.require_force == _player_force.GetForce()) )
	{
		int rst = _equipment[item::EQUIP_INDEX_FORCE_TICKET].AutoAdjust(value, pCfg->reputation_max - repu);
		if(rst == 0)
		{
			item& it = _equipment[item::EQUIP_INDEX_FORCE_TICKET];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
			
			int type = _equipment[item::EQUIP_INDEX_FORCE_TICKET].type;
			_equipment.DecAmount(item::EQUIP_INDEX_FORCE_TICKET,1);
			_runner->player_drop_item(IL_EQUIPMENT,item::EQUIP_INDEX_FORCE_TICKET,type,1,S2C::DROP_TYPE_USE);
		}
		else if(rst > 0)
		{
			PlayerGetItemInfo(IL_EQUIPMENT,item::EQUIP_INDEX_FORCE_TICKET);
		}
	}

	if(value > 0) _player_force.IncReputation(value);
	return true;
}

bool
gplayer_imp::DecForceReputation(int value)
{
	if(_player_force.GetForce() == 0) return false;
	int repu = _player_force.GetReputation();
	if(value > repu) value = repu;
	if(value <= 0) return false;
	_player_force.DecReputation(value);
	return true;
}

bool
gplayer_imp::IncForceContribution(int value)
{
	if(_player_force.GetForce() == 0) return false;
	DATA_TYPE dt;
	FORCE_CONFIG * pCfg = (FORCE_CONFIG*) world_manager::GetDataMan().get_data_ptr(_player_force.GetForce(), ID_SPACE_CONFIG,dt);
	if(dt != DT_FORCE_CONFIG || pCfg == NULL) return false;

	int contri = _player_force.GetContribution();
	if(value > pCfg->contribution_max - contri) value = pCfg->contribution_max - contri;
	if(value <= 0) return false;
	_player_force.IncContribution(value);
	return true;
}

bool
gplayer_imp::DecForceContribution(int value)
{
	if(_player_force.GetForce() == 0) return false;
	int contri = _player_force.GetContribution();
	if(value > contri) value = contri;
	if(value <= 0) return false;
	_player_force.DecContribution(value);
	return true;
}

void 
gplayer_imp::UpdateForceTicketInfo(int require_force, int repu_inc_ratio)
{
	_force_ticket_info.require_force = require_force;
	_force_ticket_info.repu_inc_ratio = repu_inc_ratio;
}

int 
gplayer_imp::CountryJoinApply()
{
	if(!CheckCoolDown(COOLDOWN_INDEX_COUNTRY_JOIN_APPLY)) return S2C::ERR_OBJECT_IS_COOLING;
	//����Ƿ��Ѿ�����ĳ������
	if(GetCountryId()) return S2C::ERR_ALREADY_JOIN_COUNTRY;
	if(GetHistoricalMaxLevel() < COUNTRYJOIN_LEVEL_REQUIRED) return S2C::ERR_LEVEL_NOT_MATCH;
	if(!IsItemExist(COUNTRYJOIN_APPLY_TICKET)) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
		
	if(!IsInTeam())
	{
		//���˱���ģʽ
		SetCoolDown(COOLDOWN_INDEX_COUNTRY_JOIN_APPLY,COUNTRY_JOIN_APPLY_COOLDOWN_TIME);
		
		GMSV::CBApplyEntry entry;
		entry.roleid = _parent->ID.id;
		entry.major_strength = Get16Por9JWeapon();
		entry.minor_strength = GetSoulPower();
		GMSV::CountryBattleApply(&entry, 1);
		return 0;
	}
	
	//��ӱ���ģʽ
	//���봦�ڶ�����ֻ�жӳ��ܷ���������
	if(!IsTeamLeader()) return S2C::ERR_NOT_TEAM_LEADER;
	for(int i=0; i<GetTeamMemberNum(); i++)
	{
		if(GetTeamMember(i).data.level < COUNTRYJOIN_LEVEL_REQUIRED) return S2C::ERR_LEVEL_NOT_MATCH;
	}

	XID list[TEAM_MEMBER_CAPACITY];
	int count = GetMemberList(list);
	ONET::Thread::Pool::AddTask(new CountryJoinApplyJob(this, list, count, COUNTRYJOIN_LEVEL_REQUIRED, COUNTRYJOIN_APPLY_TICKET));
	return 0;
}

bool 
gplayer_imp::CountryJoinStep1(int country_id, int country_expiretime, int major_strength, int minor_strength, int world_tag, const A3DVECTOR & pos)
{
	ASSERT(country_expiretime > 0 && "������Ҷ�Ӧ���й���ʱ��");
	if(GetCountryId()) return false;
	if(!IsItemExist(COUNTRYJOIN_APPLY_TICKET)) return false;
	
	if(_player_state != PLAYER_STATE_NORMAL 
			&& _player_state != PLAYER_STATE_BIND
			&& _player_state != PLAYER_SIT_DOWN
			&& _player_state != PLAYER_STATE_MARKET)
	{
		return false;
	}
	LeaveAbnormalState();

	if(world_tag == world_manager::GetWorldTag()) return false;
	//׼����������ս�׶������ͳɹ�����м�����Ҳ���
	ClearSwitchAdditionalData();
	if(LongJump(pos, world_tag))
	{
		_switch_additional_data = new countryterritory_switch_data(country_id, country_expiretime, major_strength, minor_strength);
	}
	GLog::log(GLOG_INFO,"���%d�������step1(id=%d,expire=%d)׼������(tag=%d,pos=%f,%f,%f)", _parent->ID.id, country_id, country_expiretime, world_tag, pos.x, pos.y, pos.z);
	return true;
}

bool 
gplayer_imp::CountryJoinStep2()
{
	countryterritory_switch_data * pData = substance::DynamicCast<countryterritory_switch_data>(_switch_additional_data);
	ASSERT(pData);
	if(GetCountryId()) return false;
	if(!IsItemExist(COUNTRYJOIN_APPLY_TICKET)) return false;
	
	TakeOutItem(COUNTRYJOIN_APPLY_TICKET);
	SetCountryId(pData->country_id, pData->country_expiretime);
	GMSV::CountryBattleJoin(_parent->ID.id, pData->country_id, world_manager::GetWorldTag(), pData->major_strength, pData->minor_strength, GetParent()->IsKing());
	GLog::log(GLOG_INFO,"���%d�������step2(id=%d,expire=%d)�ɹ�", _parent->ID.id, pData->country_id, pData->country_expiretime);

	ClearSwitchAdditionalData();
	return true;
}

int 
gplayer_imp::CountryLeave()
{
	if(!GetCountryId()) return S2C::ERR_NOT_JOIN_COUNTRY;

	GMSV::CountryBattleLeave(_parent->ID.id, GetCountryId(), Get16Por9JWeapon(), GetSoulPower());
	//�������ID �ȴ���ս��ͼ�߼�������߳�
	SetCountryId(0,0);
	return 0;
}

void 
gplayer_imp::CountryTerritoryMove(const A3DVECTOR & pos, bool capital)
{
	if(!world_manager::GetInstance()->IsCountryTerritoryWorld()) return;

	if(_player_state != PLAYER_STATE_NORMAL 
			&& _player_state != PLAYER_STATE_BIND
			&& _player_state != PLAYER_SIT_DOWN
			&& _player_state != PLAYER_STATE_MARKET)
	{
		return;
	}
	LeaveAbnormalState();
	LongJump(pos);
	//���׶�������ΪGM����
	object_interface obj_if(this);
	if(capital)
		obj_if.SetGMInvisibleFilter(true, -1, filter::FILTER_MASK_NOSAVE);
	else
		obj_if.SetGMInvisibleFilter(false, -1, 0);
}

void 
gplayer_imp::GetCountryKickoutPos(int & world_tag, A3DVECTOR & pos)
{
	if(InCentralServer())
	{
		world_tag = 142;
		pos = world_manager::GetCentralServerBrithPos(_src_zoneid);
	}
	else
	{
		world_tag = 1;
		pos = A3DVECTOR(1485,225,1269);
	}
}

bool
gplayer_imp::OI_TestSafeLock()
{
	return ((gplayer_controller*)_commander)->InSafeLock();
}

void
gplayer_imp::OI_TryCancelPlayerBind()
{
	if(_player_state == PLAYER_STATE_BIND)
		_bind_player.PlayerCancel(this);
}

//lgc
void gplayer_imp::UpdateCurElfInfo(unsigned int id, short refine_level, short str, short agi, short vit, short eng, const char * skilldata, int skillcnt)
{
	//�˺�������С����װ��ʱOnActivate()�е���,�˺��������ʼ��refine_effect_active,vigor
	if(_cur_elf_info.id != (unsigned int)-1)	//��ֹС���鱻�ظ�װ��
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}

	_cur_elf_info.id = id;
	_cur_elf_info.refine_level = refine_level;
	_cur_elf_info.strength = str;
	_cur_elf_info.agility = agi;
	_cur_elf_info.vitality = vit;
	_cur_elf_info.energy = eng;

	ASSERT(_cur_elf_info.skillvect.size() == 0);

	struct elf_skill_data * pskill = (struct elf_skill_data *)skilldata;
	if(skilldata != NULL && skillcnt > 0 && skillcnt <= MAX_ELF_SKILL_CNT)
		for(int i=0; i<skillcnt; i++, pskill++)
			_cur_elf_info.skillvect.push_back(*pskill);
	
	//�������Ҫ���Ӹ���_elf_en�Ĵ��룬Ȼ�������������
	UpdateElfProp();
	UpdateElfVigor();	

}

void gplayer_imp::ClearCurElfInfo()
{
	//�˺�������С����ж��ʱOnDeactivate()�е���, �˺��������ʼ��refine_effect_active,vigor
	_cur_elf_info.clear();
}

void gplayer_imp::UpdateElfProp() //��elf_enhance���µ�_cur_elf_info��final_...��
{
	_cur_elf_info.final_strength = _cur_elf_info.strength + _elf_en.str_point;	
	_cur_elf_info.final_agility = _cur_elf_info.agility + _elf_en.agi_point;	
	_cur_elf_info.final_vitality = _cur_elf_info.vitality + _elf_en.vit_point;	
	_cur_elf_info.final_energy = _cur_elf_info.energy + _elf_en.eng_point;	
	_cur_elf_info.refresh_enhance = true;
	
}

void gplayer_imp::UpdateElfVigor()
{
	_cur_elf_info.max_vigor = INITIAL_MAX_VIGOR + _cur_elf_info.final_vitality;
	_cur_elf_info.vigor_gen = INITIAL_VIGOR_GEN + _cur_elf_info.final_energy * 0.02f;
	if(_cur_elf_info.vigor > _cur_elf_info.max_vigor)
		_cur_elf_info.vigor = _cur_elf_info.max_vigor;
	_cur_elf_info.refresh_vigor = true;
	
}

void gplayer_imp::UpdateMinElfStatusValue(int value)
{
	//���õ�ʱ��֤value > 0
	if(_min_elf_status_value <= 0)
		_min_elf_status_value = value;
	else if(_min_elf_status_value > value)
		_min_elf_status_value = value;
}

void gplayer_imp::UpdateAllElfSecureStatus()
{
	_min_elf_status_value = 0;
	int t = g_timer.get_systime();
	size_t i, size;
	//���_inventory
	size = _inventory.Size();
	for(i=0; i<size; i++)
	{
		int value = _inventory[i].GetStatusValue();
		if(value <= 0) 
			continue;
		if(value <= t)
		{
			_inventory[i].UpdateElfSecureStatus();
			_inventory[i].proc_type &= ~ (item::ITEM_PROC_TYPE_NOTHROW|item::ITEM_PROC_TYPE_NOSELL|item::ITEM_PROC_TYPE_NOTRADE);
			PlayerGetItemInfo(IL_INVENTORY, i);//�ͻ��˸���
		}
		else
		{
			if(_min_elf_status_value <= 0)
				_min_elf_status_value = value;
			else if(_min_elf_status_value > value)
				_min_elf_status_value = value;
		}
	}
	//���_equipment
	size = _equipment.Size();
	for(i=0; i<size; i++)
	{
		int value = _equipment[i].GetStatusValue();
		if(value <= 0) 
			continue;
		if(value <= t)
		{
			_equipment[i].UpdateElfSecureStatus();
			_equipment[i].proc_type &= ~ (item::ITEM_PROC_TYPE_NOTHROW|item::ITEM_PROC_TYPE_NOSELL|item::ITEM_PROC_TYPE_NOTRADE);
			PlayerGetItemInfo(IL_EQUIPMENT, i);//�ͻ��˸���
		}
		else
		{
			if(_min_elf_status_value <= 0)
				_min_elf_status_value = value;
			else if(_min_elf_status_value > value)
				_min_elf_status_value = value;
		}
	}
	//���_backpack
	item_list & backpack1 = _trashbox.GetBackpack1();
	size = backpack1.Size();
	for(i=0; i<size; i++)
	{
		int value = backpack1[i].GetStatusValue();
		if(value <= 0) 
			continue;
		if(value <= t)
		{
			backpack1[i].UpdateElfSecureStatus();
			backpack1[i].proc_type &= ~ (item::ITEM_PROC_TYPE_NOTHROW|item::ITEM_PROC_TYPE_NOSELL|item::ITEM_PROC_TYPE_NOTRADE);
			PlayerGetItemInfo(IL_TRASH_BOX, i);//�ͻ��˸���
		}
		else
		{
			if(_min_elf_status_value <= 0)
				_min_elf_status_value = value;
			else if(_min_elf_status_value > value)
				_min_elf_status_value = value;
		}
	}
	//��鹫���ֿ�
	item_list & user_backpack1 = _user_trashbox.GetBackpack1();
	size = user_backpack1.Size();
	for(i=0; i<size; i++)
	{
		int value = user_backpack1[i].GetStatusValue();
		if(value <= 0) 
			continue;
		if(value <= t)
		{
			user_backpack1[i].UpdateElfSecureStatus();
			user_backpack1[i].proc_type &= ~ (item::ITEM_PROC_TYPE_NOTHROW|item::ITEM_PROC_TYPE_NOSELL|item::ITEM_PROC_TYPE_NOTRADE);
			PlayerGetItemInfo(IL_USER_TRASH_BOX, i);//�ͻ��˸���
		}
		else
		{
			if(_min_elf_status_value <= 0)
				_min_elf_status_value = value;
			else if(_min_elf_status_value > value)
				_min_elf_status_value = value;
		}
	}
}

void gplayer_imp::TriggerElfRefineEffect()
{
	//�Ƿ�װ����С����
	if(_cur_elf_info.id == (unsigned int)-1)
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	//�����ȼ�����	
	if(_cur_elf_info.refine_level <= 0 || _cur_elf_info.refine_level > MAX_ELF_REFINE_LEVEL)
	{
		_runner->error_message(S2C::ERR_ELF_REFINE_ACTIVATE_FAILED);
		return;
	}
	if(!_cur_elf_info.refine_effect_active)
	{
		//�жϵ�ǰ�����Ƿ��㹻
		int startup_cost = elf_refine_effect_table[_cur_elf_info.refine_level].std_cost * (_basic.level  + 105)/210 * 60;
		int cur_stamina = it.GetStamina();
		if(cur_stamina < startup_cost)
		{
			_runner->error_message(S2C::ERR_ELF_NOT_ENOUGH_STAMINA);
			return;
		}
		//��������
		it.DecStamina(startup_cost);
		_runner->query_elf_stamina(cur_stamina - startup_cost);//֪ͨ�ͻ���
		ElfRefineActivate(_cur_elf_info.refine_level);
		_runner->elf_refine_activate(1);
	}
	else
	{
		ElfRefineDeactivate(_cur_elf_info.refine_level);
		_runner->elf_refine_activate(0);
	}
}

void gplayer_imp::ElfRefineActivate(short refine_level)
{
	if(refine_level <= 0 || refine_level > MAX_ELF_REFINE_LEVEL)
		return;
	_en_point.max_hp += elf_refine_effect_table[refine_level].max_hp;
	_defend_degree += elf_refine_effect_table[refine_level].defend_degree;
	_attack_degree += elf_refine_effect_table[refine_level].attack_degree;
	//player ����
	property_policy::UpdateLife(this);
	PlayerGetProperty();
	SetRefreshState();
	
	gplayer * player = GetParent();
	player->object_state |= gactive_object::STATE_ELF_REFINE_ACTIVATE;
	
	_cur_elf_info.refine_effect_active = true;
}

void gplayer_imp::ElfRefineDeactivate(short refine_level)
{
	if(refine_level <= 0 || refine_level > MAX_ELF_REFINE_LEVEL)
		return;
	_en_point.max_hp -= elf_refine_effect_table[refine_level].max_hp;
	_defend_degree -= elf_refine_effect_table[refine_level].defend_degree;
	_attack_degree -= elf_refine_effect_table[refine_level].attack_degree;
	//player ����
	property_policy::UpdateLife(this);
	PlayerGetProperty();
	SetRefreshState();
	
	gplayer * player = GetParent();
	player->object_state &= ~gactive_object::STATE_ELF_REFINE_ACTIVATE;
		
	_cur_elf_info.refine_effect_active = false;
}

void gplayer_imp::ElfAddAttribute(short str, short agi, short vit, short eng)
{
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	//�����Ƿ����
	if(!it.AddAttributePoint(str, agi, vit, eng, true))
	{
		_runner->error_message(S2C::ERR_ELF_ADD_ATTRIBUTE_FAILED);
		return;
	}
	//�޸�
	it.Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);
	it.AddAttributePoint(str, agi, vit, eng, false);
	it.Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);
	PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);//�ͻ���װ������
}

void gplayer_imp::ElfAddGenius(short g0, short g1, short g2, short g3, short g4)
{
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	//�����Ƿ����
	if(!it.AddGeniusPoint(g0, g1, g2, g3, g4, true))
	{
		_runner->error_message(S2C::ERR_ELF_ADD_GENIUS_FAILED);
		return;
	}
	//�޸�
	it.Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);
	it.AddGeniusPoint(g0, g1, g2, g3, g4, false);
	it.Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);
	PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);//�ͻ���װ������
	_runner->elf_cmd_result(S2C::ELF_ADD_GENIUS, it.type, 0, 0);
}

void gplayer_imp::ElfPlayerInsertExp(unsigned int exp, char use_sp)
{
	if(!_cooldown.TestCoolDown(COOLDOWN_INDEX_ELF_CMD))
	{
		_runner->error_message(S2C::ERR_ELF_CMD_IN_COOLDOWN);
		return;	
	}
	_cooldown.SetCoolDown(COOLDOWN_INDEX_ELF_CMD, ELF_CMD_COOLDOWN_TIME);
	
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	//����ʹ������Ԫ��;�����ɽ��׵�С����ע����
	if(it.GetSecureStatus() == elf_item::STATUS_TRADABLE)
	{
		_runner->error_message(S2C::ERR_ELF_PLAYER_INSERT_EXP_FAILED);
		return;
	}
	//�����Ƿ����
	if(use_sp)
	{
		if(exp <= 0 || _basic.skill_point <= 0 || exp > (unsigned int)_basic.skill_point || exp > 840000000)
		{
			_runner->error_message(S2C::ERR_ELF_PLAYER_INSERT_EXP_FAILED);
			return;
		}
		exp *= 5; 
	}
	else
	{
		if(exp <= 0 || _basic.exp <= 0 || exp > (unsigned int)_basic.exp)
		{
			_runner->error_message(S2C::ERR_ELF_PLAYER_INSERT_EXP_FAILED);
			return;
		}
	}
	bool is_levelup = false;
	if(it.InsertExp(exp, GetHistoricalMaxLevel(), this, is_levelup, true) == (unsigned int)-1)//����player level>=elf level
	{
		_runner->error_message(S2C::ERR_ELF_PLAYER_INSERT_EXP_FAILED);
		return;
	}
	//�޸�	
	it.Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);	//����û�б�Ҫ�ӣ�������������������ʽ��һ��
	unsigned int _exp = it.InsertExp(exp, GetHistoricalMaxLevel(), this, is_levelup, false);
	it.Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);	//����û�б�Ҫ�ӣ�������������������ʽ��һ��
	//player����_exp
	if(use_sp)
		_basic.skill_point -= (_exp%5 ? (int)(_exp/5+1) : (int)(_exp/5));
	else
		_basic.exp -= (int)_exp;
	SetRefreshState();
	GLog::log(GLOG_INFO,"�û�%d��%d��%s��С����ע����,���о���%d,Ԫ��%d", _parent->ID.id, (use_sp?_exp/5:_exp), (use_sp?"Ԫ��":"����"), _basic.exp, _basic.skill_point);
	if(is_levelup)
		PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);//�ͻ���װ������
}

unsigned int gplayer_imp::ElfInsertExpUsePill(unsigned int exp, int exp_level)
{
	if(!_cooldown.TestCoolDown(COOLDOWN_INDEX_ELF_CMD))
	{
		_runner->error_message(S2C::ERR_ELF_CMD_IN_COOLDOWN);
		return (unsigned int)-1;	
	}
	_cooldown.SetCoolDown(COOLDOWN_INDEX_ELF_CMD, ELF_CMD_COOLDOWN_TIME);
	
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	//�����Ƿ����
	if(exp <= 0) return (unsigned int)-1;
	bool is_levelup = false;
	if(it.InsertExp(exp, exp_level, this, is_levelup, true) == (unsigned int)-1)//����player level>=elf level
	{
		_runner->error_message(S2C::ERR_ELF_INSERT_EXP_USE_PILL_FAILED);
		return (unsigned int)-1;
	}
	//�޸�	
	it.Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);	//����û�б�Ҫ�ӣ�������������������ʽ��һ��
	unsigned int _exp = it.InsertExp(exp, exp_level, this, is_levelup, false);
	it.Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);	//����û�б�Ҫ�ӣ�������������������ʽ��һ��
	if(is_levelup)
		PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);//�ͻ���װ������
	return _exp;
}

void gplayer_imp::ElfReceiveExp(unsigned int exp)
{
	if(!exp) return;
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	
	bool is_levelup = false;
	if(it.InsertExp(exp, GetHistoricalMaxLevel(), this, is_levelup, false) == (unsigned int)-1) return;
	if(is_levelup)
		PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);
}

void gplayer_imp::ElfEquipItem(size_t index_inv)
{
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	if(index_inv >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it1 = _inventory[index_inv];
	if(it1.type <= 0 || it1.body == NULL || it1.body->GetItemType() != item_body::ITEM_TYPE_ELF_EQUIP)
	{
		_runner->error_message(S2C::ERR_ELF_EQUIP_ITEM_FAILED);
		return;
	}
	//�����Ƿ����
	if(!it.EquipElfItem(it1.type, true))
	{
		_runner->error_message(S2C::ERR_ELF_EQUIP_ITEM_FAILED);
		return;
	}
	//�޸�
	it.Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);
	it.EquipElfItem(it1.type, false);
	it.Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);
	//��Ʒ��������Ʒ
	_runner->player_drop_item(IL_INVENTORY, index_inv, it1.type, 1, S2C::DROP_TYPE_TAKEOUT);
	_inventory.DecAmount(index_inv, 1);
	PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);//�ͻ���װ������
	_runner->elf_cmd_result(S2C::ELF_EQUIP_ITEM, it.type, it1.type, 0);
}

void gplayer_imp::ElfChangeSecureStatus(int status)
{
	if(!_cooldown.TestCoolDown(COOLDOWN_INDEX_ELF_CMD))
	{
		_runner->error_message(S2C::ERR_ELF_CMD_IN_COOLDOWN);
		return;	
	}
	_cooldown.SetCoolDown(COOLDOWN_INDEX_ELF_CMD, ELF_CMD_COOLDOWN_TIME);
	
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	//�����Ƿ����
	if(!it.ChangeElfSecureStatus(status, true))
	{
		_runner->error_message(S2C::ERR_ELF_CHANGE_SECURE_STATUS_FAILED);
		return;
	}
	//�޸�
	it.Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);	//û�ñ���������һ��
	it.ChangeElfSecureStatus(status, false);
	it.Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);	//û�ñ���������һ��
	//
	if(status == elf_item::STATUS_SECURE)
		it.proc_type |= (item::ITEM_PROC_TYPE_NOTHROW|item::ITEM_PROC_TYPE_NOSELL|item::ITEM_PROC_TYPE_NOTRADE);
	else if(status == elf_item::STATUS_TRANSFORM) 
		UpdateMinElfStatusValue(it.GetStatusValue());//����_min_elf_status_value
	
	PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);//�ͻ���װ������
}

void gplayer_imp::CastElfSkill(unsigned short skill_id, char force_attack, int target_count, int * targets)
{
	//�Ƿ�װ����С����
	if(_cur_elf_info.id == (unsigned int)-1)
	{
		_runner->error_message(S2C::ERR_CAST_ELF_SKILL_FAILED);
		return;
	}
	//��ǰС���� �Ƿ���ü��ܣ�id level��
	short skill_level;
	size_t i, size = _cur_elf_info.skillvect.size();
	for(i=0; i<size; i++)
	{
		if(_cur_elf_info.skillvect[i].id == skill_id) 
		{
			ASSERT(_cur_elf_info.skillvect[i].level > 0);
			skill_level = _cur_elf_info.skillvect[i].level;
			break;
		}
	}
	if(i == size)
	{
		_runner->error_message(S2C::ERR_CAST_ELF_SKILL_FAILED);
		return;
	}
	//���С���鼼�ܵĹ�����ȴ
	if(!_cooldown.TestCoolDown(COOLDOWN_INDEX_CAST_ELF_SKILL))
	{
		_runner->error_message(S2C::ERR_CAST_ELF_SKILL_IN_COOLDOWN);
		return;	
	}
	//��鼼�ܵ���ȴ
	int cd_id = GNET::SkillWrapper::GetCooldownID(skill_id);
	if(!_cooldown.TestCoolDown(cd_id))
	{
		_runner->error_message(S2C::ERR_CAST_ELF_SKILL_IN_COOLDOWN);
		return;	
	}
	//ʩ��
	SKILL::Data _data(0);
	_data.id = skill_id;
	_data.forceattack = force_attack;
	abase::vector<XID, abase::fast_alloc<> > _target_list;
	if(target_count > 0)
	{
		XID id;
		_target_list.reserve(target_count);
		for(int i=0; i<target_count; i++, targets++)
		{
			MAKE_ID(id, *targets);
			_target_list.push_back(id);
		}
	}
	if( _skill.RunElfSkill(_data, skill_level, object_interface(this), _target_list.begin(), _target_list.size()) != 0)
	{
		_runner->error_message(S2C::ERR_CAST_ELF_SKILL_FAILED);
		return;	
	}
	else	//�ɹ������蹫����ȴ
		SetCoolDown(COOLDOWN_INDEX_CAST_ELF_SKILL, CAST_ELF_SKILL_COOLDOWN_TIME);
	
}

bool gplayer_imp::RechargeEquippedElf(size_t index, size_t count)	//��ɽ��������߼���ȫ��ͬ
{
	if(count == 0 || index >= _inventory.Size()) return false;
	item & element = _inventory[index];
	if(element.type == -1 || element.count < count) return false;
	item & it = _equipment[item::EQUIP_INDEX_ELF];//��С�����
	if(it.type == -1 || !it.body) return false;
	int element_id = element.type;
	DATA_TYPE dt;
	ELEMENT_ESSENCE * ess = (ELEMENT_ESSENCE*)world_manager::GetDataMan().get_data_ptr((unsigned int)element_id, ID_SPACE_ESSENCE,dt);
	if( !ess || dt != DT_ELEMENT_ESSENCE) return false;
	int old_stamina = it.GetStamina();
	int cur_stamina;
	int rst = it.body->Recharge(ess->level,count,cur_stamina);
	if(rst > 0)
	{
		UpdateMallConsumptionDestroying(element.type, element.proc_type, rst);

		_inventory.DecAmount(index,rst);
		_runner->player_drop_item(IL_INVENTORY,index,element_id,rst,S2C::DROP_TYPE_RECHARGE);
		_runner->query_elf_stamina(cur_stamina);	//֪ͨ�ͻ���С���������仯
		_runner->elf_cmd_result(S2C::ELF_RECHARGE, element_id, rst, cur_stamina - old_stamina);
	}
	return true;
}

void gplayer_imp::ElfDecAttribute(size_t inv_idx_elf, size_t inv_idx_ticket, short str, short agi, short vit,  short eng)
{
	//����inv_idx_ticket����
	if(inv_idx_elf >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it1 = _inventory[inv_idx_elf];
	if(it1.type <= 0 || it1.body == NULL || it1.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_DEC_ATTRIBUTE_FAILED);
		return;
	}
	int req_ticket = str + agi + vit + eng;
	int ticket1_count = GetItemCount(ELF_DEC_ATTRIBUTE_TICKET_ID);
	int ticket2_count = GetItemCount(ELF_DEC_ATTRIBUTE_TICKET_ID2);
	if(ticket1_count + ticket2_count < req_ticket)
	{
		_runner->error_message(S2C::ERR_ELF_DEC_ATTRIBUTE_FAILED);
		return;
	}
	if(!it1.DecAttributePoint(str, agi, vit, eng))
	{
		_runner->error_message(S2C::ERR_ELF_DEC_ATTRIBUTE_FAILED);
		return;
	}
	else
	{
		if(ticket1_count >= req_ticket)
			RemoveItems(ELF_DEC_ATTRIBUTE_TICKET_ID ,req_ticket, S2C::DROP_TYPE_USE, true);
		else
		{
			RemoveItems(ELF_DEC_ATTRIBUTE_TICKET_ID ,ticket1_count, S2C::DROP_TYPE_USE, true);
			RemoveItems(ELF_DEC_ATTRIBUTE_TICKET_ID2 ,req_ticket - ticket1_count, S2C::DROP_TYPE_USE, true);	
		}
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);//�ͻ���װ������
		int param1 = agi, param2 = eng;
		param1 <<= 16, param2 <<=16;
		param1 |= str, param2 |= vit;
		_runner->elf_cmd_result(S2C::ELF_DEC_ATTRIBUTE, it1.type, param1, param2);
		return;
	}
}

void gplayer_imp::ElfFlushGenius(size_t inv_idx_elf, size_t inv_idx_ticket)
{
	if(inv_idx_elf >= _inventory.Size() || inv_idx_ticket >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it1 = _inventory[inv_idx_elf];
	if(it1.type <= 0 || it1.body == NULL || it1.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_FLUSH_GENIUS_FAILED);
		return;
	}
	item & it2 = _inventory[inv_idx_ticket];
	if(it2.type != ELF_FLUSH_GENIUS_TICKET_ID && it2.type != ELF_FLUSH_GENIUS_TICKET_ID2 
		|| it2.count < 1)
	{
		_runner->error_message(S2C::ERR_ELF_FLUSH_GENIUS_FAILED);
		return;
	}
	if(!it1.FlushGeniusPoint())
	{
		_runner->error_message(S2C::ERR_ELF_FLUSH_GENIUS_FAILED);
		return;
	}
	else
	{
		UpdateMallConsumptionDestroying(it2.type, it2.proc_type, 1);

		_runner->player_drop_item(IL_INVENTORY, inv_idx_ticket, it2.type, 1, S2C::DROP_TYPE_USE);
		_inventory.DecAmount(inv_idx_ticket, 1);
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);//�ͻ���װ������
		return;
	}
}

void gplayer_imp::ElfLearnSkill(size_t inv_idx_elf, unsigned short skill_id)
{
	if(inv_idx_elf >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it = _inventory[inv_idx_elf];
	if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_LEARN_SKILL_FAILED);
		return;
	}
	int new_level;
	if( (new_level = it.LearnSkill(this, skill_id)) < 0)
	{
		_runner->error_message(S2C::ERR_ELF_LEARN_SKILL_FAILED);
		return;
	}
	else
	{
		//��С���鼼����Ҫ���ļ����飬��LearnSkill()��ɾ����֪ͨ�ͻ���
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);//�ͻ���װ������
		_runner->elf_cmd_result(S2C::ELF_LEARN_SKILL, it.type, skill_id, new_level);
		return;
	}
	
}

void gplayer_imp::ElfForgetSkill(size_t inv_idx_elf, unsigned short skill_id, short forget_level)
{
	//��С����
	if(inv_idx_elf >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it = _inventory[inv_idx_elf];
	if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_FORGET_SKILL_FAILED);
		return;
	}
	if(it.ForgetSkill(this, skill_id, forget_level) < 0)
	{
		_runner->error_message(S2C::ERR_ELF_FORGET_SKILL_FAILED);
		return;
	}
	else
	{
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);//�ͻ���װ������
		_runner->elf_cmd_result(S2C::ELF_FORGET_SKILL, it.type, skill_id, forget_level);
		return;
	}
	
}

void gplayer_imp::ElfRefine(size_t inv_idx_elf, size_t inv_idx_ticket, int ticket_cnt)
{
//���λõ��ɷŶ��������ֻ�ܷ�1������Ŷ࣬��ʵ�����ļ��㣬�����ش���
	if(inv_idx_elf >= _inventory.Size() || inv_idx_ticket >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	//��С����
	item & it = _inventory[inv_idx_elf];
	if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_REFINE_CANNOT_REFINE);
		return;
	}
	//��ǰ�����ȼ����ܾ���?
	short elf_ref_lvl = it.GetRefineLevel();
	if(elf_ref_lvl < 0 || elf_ref_lvl >= MAX_ELF_REFINE_LEVEL)
	{
		_runner->error_message(S2C::ERR_ELF_REFINE_CANNOT_REFINE);
		return;
	}
	//�Ǿ������ϣ�ȷ��ʹ������
	int ticket_id = -1;
	item & it2 = _inventory[inv_idx_ticket];
	if(it2.type == ELF_REFINE_TICKET0_ID && it2.count >= 1)
		ticket_cnt = 1;
	else if(it2.type == ELF_REFINE_TICKET1_ID && it2.count >= 1)
		ticket_cnt = 1;
	else if(it2.type == ELF_REFINE_TICKET2_ID && it2.count >= 1)
		ticket_cnt = 1;
	else if(it2.type == ELF_REFINE_TICKET3_ID && ticket_cnt > 0)
	{
		if(ticket_cnt > elf_refine_max_use_ticket3[elf_ref_lvl+1])
			ticket_cnt = elf_refine_max_use_ticket3[elf_ref_lvl+1];
		//����Ӧ�ü����������е�����
		if(!CheckItemExist(it2.type, ticket_cnt))
		{
			_runner->error_message(S2C::ERR_ELF_REFINE_CANNOT_REFINE);
			return;
		}
	}
	else
	{
		_runner->error_message(S2C::ERR_ELF_REFINE_CANNOT_REFINE);
		return;
	}
	ticket_id = it2.type;
	
	//����
	int level_original = 0;
	int rst = it.ElfRefine(ticket_id, ticket_cnt, level_original);
	if(rst != item::REFINE_CAN_NOT_REFINE)
	{
		const char * tbuf[] = {"�ɹ�", "�޷�����" , "������ʧ", "���Խ���һ��", "���Ա���", "װ������","δ֪1","δ֪2","δ֪3"};
		GLog::log(GLOG_INFO,"�û�%d����С����(%d)[%s]������ǰ����%d ����С���龫������(%d)%d��",_parent->ID.id, it.type, tbuf[rst],level_original, ticket_id, ticket_cnt);
		if(level_original >= 6)
		{
			GLog::refine(_parent->ID.id,it.type, level_original, rst, ticket_cnt);
		}
	}
	switch(rst)
	{
		case item::REFINE_CAN_NOT_REFINE:
		//�޷����о��������;���ʧ��  ������������κα仯 
		_runner->error_message(S2C::ERR_ELF_REFINE_CANNOT_REFINE);
		return;
		break;

		case item::REFINE_SUCCESS:
		//�����ɹ�
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);
		RemoveItems(ticket_id, ticket_cnt, S2C::DROP_TYPE_USE, true);
		_runner->elf_cmd_result(S2C::ELF_REFINE, it.type, 0, level_original);
		break;

		case item::REFINE_FAILED_LEVEL_0:
		//����һ��ʧ�ܣ�ɾ������
		RemoveItems(ticket_id, ticket_cnt, S2C::DROP_TYPE_USE, true);
		_runner->elf_cmd_result(S2C::ELF_REFINE, it.type, 1, level_original);
		break;

		case item::REFINE_FAILED_LEVEL_1:
		//��������ʧ�ܣ�ɾ�����ϣ�����һ�� �����Ѿ����Զ�����
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);
		RemoveItems(ticket_id, ticket_cnt, S2C::DROP_TYPE_USE, true);
		_runner->elf_cmd_result(S2C::ELF_REFINE, it.type, 2, level_original);
		break;

		case item::REFINE_FAILED_LEVEL_2:
		//��������ʧ�ܣ�ɾ�����ϣ������Ѿ����Զ����
		PlayerGetItemInfo(IL_INVENTORY,inv_idx_elf);
		RemoveItems(ticket_id, ticket_cnt, S2C::DROP_TYPE_USE, true);
		_runner->elf_cmd_result(S2C::ELF_REFINE, it.type, 3,level_original);
		break;
		
		default:
		GLog::log(GLOG_ERR,"����װ��ʱ�������쳣����%d",rst);
	}

	return;

}

void gplayer_imp::ElfRefineTransmit(size_t inv_idx_src, size_t inv_idx_dst)
{
	//�Ƿ���С���飿
	if(inv_idx_src == inv_idx_dst || inv_idx_src >= _inventory.Size() || inv_idx_dst >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & src_item = _inventory[inv_idx_src];
	item & dst_item = _inventory[inv_idx_dst];
	if(src_item.type <= 0 || src_item.body == NULL || src_item.body->GetItemType() != item_body::ITEM_TYPE_ELF
		|| dst_item.type <= 0 || dst_item.body == NULL || dst_item.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_REFINE_TRANSMIT_FAILED);
		return;
	}

	//���С����İ�ȫ״̬����
	bool src_tradable = (src_item.GetSecureStatus() == elf_item::STATUS_TRADABLE);
	bool dst_tradable = (dst_item.GetSecureStatus() == elf_item::STATUS_TRADABLE);
	if(!src_tradable && dst_tradable)
	{
		_runner->error_message(S2C::ERR_ELF_REF_TRANS_FROM_TRADE_TO_UNTRADE);
		return;
	}

	//�������װ���ľ����ȼ���ƥ���
	short src_level = src_item.GetRefineLevel();
	short dst_level = dst_item.GetRefineLevel();
	if(src_level < 0 || dst_level < 0 || src_level > MAX_ELF_REFINE_LEVEL || dst_level > MAX_ELF_REFINE_LEVEL 
		||src_level == 0 || dst_level >= src_level) 
	{
		_runner->error_message(S2C::ERR_ELF_REFINE_TRANSMIT_FAILED);
		return;
	}
	
	//���Ǭ��ʯ�������Ƿ��㹻
	if(!CheckItemExist(ELF_REFINE_TRANSMIT_TICKET_ID, elf_refine_transmit_cost[src_level]))
	{
		_runner->error_message(S2C::ERR_TRANSMIT_REFINE_NO_MATERIAL);
		return;
	}
	//����˫�����¾����ȼ�
	short n_src_level = src_item.SetRefineLevel(0);
	short n_dst_level = dst_item.SetRefineLevel(src_level);
	
	//����־
	GLog::log(GLOG_INFO,"���%dִ���˾�����ת��������(%d[%d->%d])��(%d[%d->%d])",_parent->ID.id, 
			src_item.type, src_level, n_src_level, 
			dst_item.type,dst_level,n_dst_level);

	//�۳�Ǭ��ʯ
	RemoveItems(ELF_REFINE_TRANSMIT_TICKET_ID, elf_refine_transmit_cost[src_level], S2C::DROP_TYPE_USE, true);
	
	//֪ͨ�ͻ������ݸ���
	PlayerGetItemInfo(IL_INVENTORY,inv_idx_src);
	PlayerGetItemInfo(IL_INVENTORY,inv_idx_dst);
	
}

void gplayer_imp::ElfDecompose(size_t inv_idx_elf)
{
	//���С�����Ƿ����
	if(inv_idx_elf >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it = _inventory[inv_idx_elf];
	if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_DECOMPOSE_FAILED);
		return;
	}
	//���С�����Ƿ�ɽ���
	if(it.GetLevel() > 40 && it.GetSecureStatus() != elf_item::STATUS_TRADABLE)
	{
		_runner->error_message(S2C::ERR_DECOMPOSE_UNTRADABLE_ELF);
		return;
	}
	//��ȡ�ɷֽ⾭��
	unsigned int exp = 0;
	int exp_level = 0;
	if(!it.GetDecomposeElfExp(exp, exp_level) || exp <= 0 || exp_level <= 0)
	{
		_runner->error_message(S2C::ERR_ELF_DECOMPOSE_EXP_ZERO);
		return;
	}
	//���ɾ�����item_data
	element_data::item_tag_t tag = {element_data::IMT_NULL,0};
	item_data * data = world_manager::GetDataMan().generate_item_from_player(ELF_EXPPILL_ID, &tag, sizeof(tag));	
	if(data == NULL)
	{
		_runner->error_message(S2C::ERR_ELF_DECOMPOSE_FAILED);
		return;
	}
	*(unsigned int *)(data->item_content) = exp;
	*((int *)(data->item_content)+1) = exp_level;
	GLog::log(GLOG_INFO,"�û�%d�ֽ�С����%d�õ�С���龭����%dexp=%uexp_level=%d",_parent->ID.id, it.type, data->type, exp, exp_level);
	//����С����
	_runner->player_drop_item(IL_INVENTORY, inv_idx_elf, it.type, 1, S2C::DROP_TYPE_DECOMPOSE);
	//int elf_id = it.type;		//����һ��С����id
	_inventory.DecAmount(inv_idx_elf, 1);
	//��������������
	int rst = _inventory.PushInEmpty(0, *data, 1);
	if(rst >= 0)
	{
		_runner->obtain_item(data->type, data->expire_date, 1, _inventory[rst].count, IL_INVENTORY, rst);
		__PRINTF("��Ʒid:%d  ����������%d slot����%d λ��%d\n",data->type,1,_inventory[rst].count,rst);
		_runner->elf_cmd_result(S2C::ELF_DECOMPOSE, ELF_EXPPILL_ID, exp_level, exp);
	}
	else
	{
		ASSERT(false && "����������û�пռ�");
		FreeItem(data);
		return;
	}
	
	FreeItem(data);
	return;
}
void gplayer_imp::ElfDestroyItem(size_t inv_idx_elf, int mask, size_t inv_idx_equip)
{
	//���С�����Ƿ����
	if(inv_idx_elf >= _inventory.Size())
	{
		_runner->error_message(S2C::ERR_FATAL_ERR);
		return;
	}
	item & it = _inventory[inv_idx_elf];
	if(it.type <= 0 || it.body == NULL || it.body->GetItemType() != item_body::ITEM_TYPE_ELF)
	{
		_runner->error_message(S2C::ERR_ELF_DESTROY_ITEM_FAILED);
		return;
	}
	//���mask
	if(mask != 0x01 && mask != 0x02 && mask != 0x04 && mask != 0x08)
	{
		_runner->error_message(S2C::ERR_ELF_DESTROY_ITEM_FAILED);
		return;
	}
	//���inv_idx_equip
	int equip_type = -1;
	if(inv_idx_equip != 255)
	{
		if(inv_idx_equip >= _inventory.Size())
		{
			_runner->error_message(S2C::ERR_FATAL_ERR);
			return;
		}
		item & it2 = _inventory[inv_idx_equip];
		if(it2.type <= 0 || it2.body == NULL || it2.body->GetItemType() != item_body::ITEM_TYPE_ELF_EQUIP)
		{
			_runner->error_message(S2C::ERR_ELF_DESTROY_ITEM_FAILED);
			return;
		}
		equip_type = it2.type;
	}
	//���٣��ͻ��˸���
	int old_equip_type = it.DestroyElfItem(mask,equip_type);
	if(old_equip_type > 0)
	{
		if(equip_type != -1){
			item & it = _inventory[inv_idx_equip];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);

			_runner->player_drop_item(IL_INVENTORY, inv_idx_equip, equip_type, 1, S2C::DROP_TYPE_TAKEOUT);
			_inventory.DecAmount(inv_idx_equip, 1);
		}
		PlayerGetItemInfo(IL_INVENTORY, inv_idx_elf);
		_runner->elf_cmd_result(S2C::ELF_DESTROY_ITEM, it.type, old_equip_type, equip_type);
	}
	else
		_runner->error_message(S2C::ERR_ELF_DESTROY_ITEM_FAILED);

	return;
}

void gplayer_imp::UpdateStallInfo(int id, int max_sell, int max_buy, int max_name) //�ڰ�̯ƾ֤װ��ʱ����	
{
	ASSERT(_stall_info.stallcard_id == -1);
	_stall_info.stallcard_id = id;
	_stall_info.max_sell_slot = (max_sell>PLAYER_MARKET_MAX_SELL_SLOT ? PLAYER_MARKET_MAX_SELL_SLOT : max_sell);	
	_stall_info.max_buy_slot = (max_buy>PLAYER_MARKET_MAX_BUY_SLOT ? PLAYER_MARKET_MAX_BUY_SLOT : max_buy);	
	_stall_info.max_name_length = (max_name>PLAYER_MARKET_MAX_NAME_LEN ? PLAYER_MARKET_MAX_NAME_LEN : max_name);	
}

void gplayer_imp::ClearStallInfo()	//�ڰ�̯ƾ֤ж��ʱ����
{
	ASSERT(_stall_info.stallcard_id != -1);
	_stall_info.stallcard_id = -1;
	_stall_info.max_sell_slot = PLAYER_MARKET_SELL_SLOT;	
	_stall_info.max_buy_slot = PLAYER_MARKET_BUY_SLOT;	
	_stall_info.max_name_length = PLAYER_MARKET_NAME_LEN;	
}

void gplayer_imp::OnStallCardTakeOut()
{
	if(_player_state == PLAYER_STATE_MARKET)
	{
		ASSERT(_stall_obj != NULL);
		if((int)_stall_obj->GetSellSlot() > _stall_info.max_sell_slot
			|| (int)_stall_obj->GetBuySlot() > _stall_info.max_buy_slot
			|| (int)_stall_obj->GetNameLen() > _stall_info.max_name_length)
			CancelPersonalMarket();
	}
}

//obj_interface�ӿ�	
bool gplayer_imp::OI_GetElfProp(short& level, short& str, short& agi, short& vit, short& eng)
{
	if(_cur_elf_info.id == (unsigned int)-1)
		return false;
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	level = it.GetLevel() & 0xFF;
	if(level <=0)	return false;
	str = _cur_elf_info.final_strength;
	agi = _cur_elf_info.final_agility;
	vit = _cur_elf_info.final_vitality;
	eng = _cur_elf_info.final_energy;
	
	return true;
}
bool gplayer_imp::OI_DrainElfVigor(int dec)
{
	if(_cur_elf_info.id == (unsigned int)-1)
		return false;
	if((int)_cur_elf_info.vigor < dec)
		dec = (int)_cur_elf_info.vigor;
	_cur_elf_info.vigor -= dec;
	_cur_elf_info.refresh_vigor = true;
	return true;
}
bool gplayer_imp::OI_DrainElfStamina(int dec)
{
	if(_cur_elf_info.id == (unsigned int)-1)
		return false;
	item & it = _equipment[item::EQUIP_INDEX_ELF];
	int cur_stamina;
	if( (cur_stamina = it.GetStamina()) < dec)
		dec = cur_stamina;
	it.DecStamina(dec);
	_runner->query_elf_stamina(cur_stamina - dec);
	return true;
}
//debug only
void gplayer_imp::dump_elf_info()
{
	printf("---------------------------------------------------------------\n");
	printf("player uid: --%d-- _cur_elf_info:\n", _parent->ID.id);
	printf("id=%d refresh_vigor=%d refresh_enhance=%d refine_effect_active=%d refine_level=%d strength=%d agility=%d vitality=%d energy=%d final_strength=%d final_agility=%d final_vitality=%d final_energy=%d vigor=%f max_vigor=%f  vigor_gen=%f _min_elf_status_value=%d\n",
	_cur_elf_info.id ,_cur_elf_info.refresh_vigor ,_cur_elf_info.refresh_enhance ,_cur_elf_info.refine_effect_active ,_cur_elf_info.refine_level ,_cur_elf_info.strength ,_cur_elf_info.agility ,_cur_elf_info.vitality ,_cur_elf_info.energy ,_cur_elf_info.final_strength ,_cur_elf_info.final_agility ,_cur_elf_info.final_vitality ,_cur_elf_info.final_energy ,_cur_elf_info.vigor ,_cur_elf_info.max_vigor  ,_cur_elf_info.vigor_gen, _min_elf_status_value);
	printf("active skill:\n");
	for(size_t i=0; i<_cur_elf_info.skillvect.size(); i++)
		printf("skill[%d] id=%d level=%d", i, _cur_elf_info.skillvect[i].id, _cur_elf_info.skillvect[i].level);
	printf("\n");
	if(_cur_elf_info.id != (unsigned int)-1)
		((class elf_item*)_equipment[item::EQUIP_INDEX_ELF].body)->dump_all();
	printf("---------------------------------------------------------------\n");
}

void gplayer_imp::get_mall_detail()
{
	netgame::mall & __mall = world_manager::GetPlayerMall();	
	//���ܷ����仯����Ʒ�б�	
	abase::vector<netgame::mall::index_node_t, abase::fast_alloc<> > & __limit_goods = __mall.GetLimitGoods();
	size_t __limit_goods_count = __limit_goods.size();
	
	char buf[120]={0};
	int len;
	for(size_t i=0; i<__limit_goods_count; i++)
	{
		int index = __limit_goods[i]._index;
		netgame::mall::node_t node = __limit_goods[i]._node;
		Say("----------\n");
		memset(buf, 0, sizeof(buf));
		sprintf(buf,"index%d item%d count%d:\n",index,node.goods_id,node.goods_count);
		Say(buf);
		for(int j=0; j<4; j++)
		{
			int price = node.entry[j].cash_need;
			if(price <= 0)
				break;
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "(%d) price:%d.%02d ", j, price/100, price%100);
			Say(buf);
			int type = node.entry[j]._sale_time.GetType();
			int param1 = node.entry[j]._sale_time.GetParam1();
			int param2 = node.entry[j]._sale_time.GetParam2();
			int f;
			switch(type)
			{
				case netgame::mall::sale_time::TYPE_NOLIMIT:
					memset(buf, 0, sizeof(buf));
					sprintf(buf, "saletime:NOLIMIT\n");
					Say(buf);
					break;
				case netgame::mall::sale_time::TYPE_INTERZONE:
					memset(buf, 0, sizeof(buf));
					sprintf(buf, "saletime:INTERZONE: ");
					len = strlen(buf);
					if(param1)
						ctime_r((time_t*)&param1, buf+len);
					else
						buf[len]='0';
					len = strlen(buf);
					buf[len] = '-';
					buf[len+1] = '-';
					len = strlen(buf);
					if(param2)
						ctime_r((time_t*)&param2, buf+len);
					else
						buf[len]='0';
					Say(buf);
					break;
				break;
				case netgame::mall::sale_time::TYPE_WEEK:
					memset(buf, 0, sizeof(buf));
					sprintf(buf, "saletime:WEEK: ");
					f = 1;
					for(int m=0; m<7; m++, f<<=1)
					{
						if(param1 & f)
						{
							len = strlen(buf);
							sprintf(buf+len, "%d ",m);
						}
					}
					len = strlen(buf);
					sprintf(buf+len, "\n");
					Say(buf);
					break;
				break;
				case netgame::mall::sale_time::TYPE_MONTH:
					memset(buf, 0, sizeof(buf));
					sprintf(buf, "saletime:MONTH: ");
					f = 2;
					for(int m=1; m<32; m++, f<<=1)
					{
						if(param1 & f)
						{
							len = strlen(buf);
							sprintf(buf+len, "%d ",m);
						}
					}
					len = strlen(buf);
					sprintf(buf+len, "\n");
					Say(buf);
					break;
				break;
				default:
				break;
			}
		}
	}
	
}
void gplayer_imp::change_elf_property(int index, int value)
{
	if(_cur_elf_info.id != (unsigned int)-1)
	{
		_equipment[item::EQUIP_INDEX_ELF].Deactivate(item::BODY, item::EQUIP_INDEX_ELF, this);
		((class elf_item*)_equipment[item::EQUIP_INDEX_ELF].body)->change_elf_property(index, value, this);
		_equipment[item::EQUIP_INDEX_ELF].Activate(item::BODY, _equipment, item::EQUIP_INDEX_ELF, this);
		PlayerGetItemInfo(IL_EQUIPMENT, item::EQUIP_INDEX_ELF);
		int id1 = _equipment[item::EQUIP_INDEX_ELF].type | _equipment[item::EQUIP_INDEX_ELF].GetIdModify();
		CalcEquipmentInfo();
		_runner->equipment_info_changed(1ULL<<item::EQUIP_INDEX_ELF , 0, &id1,sizeof(id1));//�˺���ʹ����CalcEquipmentInfo�Ľ��
	}
}

void gplayer_imp::change_factionfortress(int index, int value)
{
	if(!OI_IsMafiaMember()) return;
	
	world_data_ctrl * pCtrl = _plane->w_ctrl;
	if(!pCtrl) return;
	if(pCtrl->GetFactionId() != OI_GetMafiaID()) return;
	
	faction_world_ctrl * ctrl = (faction_world_ctrl *)pCtrl;
	switch(index)
	{
		case 1:
			ctrl->exp += value;
			ctrl->exp_today += value;
			break;
		case 2:
			for(size_t i=0; i<faction_world_ctrl::MATERIAL_COUNT; i++)
				ctrl->material[i] += value;
			break;
		default:
			break;
	}
	
	PlayerGetFactionFortressInfo();
}

void
gplayer_imp::GetAUMailTask(int level,char ex_reward)
{
	PlayerTaskInterface  task_if(this);
	OnTaskExternEvent(&task_if,level);
	if(ex_reward)
	{
		OnTaskExternEvent(&task_if,EX_TK_SENDAUMAIL_EXAWARD);
	}
}

void
gplayer_imp::PlayerProduce4Choose(bool remain)
{
	if (!_cur_session || _cur_session->GetRunTimeClass()->GetGUID() != CLS_SESSION_PRODUCE4 || _cur_session->_session_id < 0)
	{
		return;
	}
	((session_produce4*)_cur_session)->ChooseItem(remain);
}

bool
gplayer_imp::Produce4ChooseExec(const recipe_template & rt, int equip_id, int equip_inv_idx, char inherit_type, void **pItem, unsigned short crc, int eq_refine_level, int eq_socket_count, int eq_stone_type[], addon_data eq_engrave_addon_list[3], size_t eq_engrave_addon_count)
{
	//�ȼ��ɵ�װ�����ڲ��ڣ�Ҫ�ǲ������Ǿͻ�ɾ��ʧ�ܣ���������������Ʒ
	if(!_inventory.IsItemExist(equip_inv_idx, equip_id, 1)) 
	{
		return false;
	}
	item & eq_it = _inventory[equip_inv_idx];
	if(eq_it.body == NULL) return false;
	if (eq_it.GetCRC() != crc) // ��Ȼλ�ú�idһ�������Ǳ���һ�������һ��װ����
	{
		return false;
	}
	//ɾ����װ��
	item& it = _inventory[equip_inv_idx];
	UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);

	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,equip_inv_idx,equip_id,1,S2C::DROP_TYPE_PRODUCE);
	_inventory.DecAmount(equip_inv_idx, 1);

	//�������������Ʒ
	item_data *data = (item_data *)(*pItem);
	int rst = _inventory.Push(*data);
	if(rst >=0)
	{
	    FirstAcquireItem(data);

		item & target_eq_it = _inventory[rst];
		if(inherit_type & PRODUCE_INHERIT_REFINE)
		{
			//�̳д�����װ���ľ���
			if(eq_refine_level > 0)
			{
				int material_need;
				int refine_addon = world_manager::GetDataMan().get_item_refine_addon(target_eq_it.type, material_need);
				if(material_need > 0 && refine_addon > 0)
					target_eq_it.body->SetRefineLevel(refine_addon,eq_refine_level);		
			}
		}
		if(inherit_type & PRODUCE_INHERIT_SOCKET)
		{
			//�̳д�����װ���Ŀ���
			//�̳д�����װ���ı�ʯ
			if(eq_socket_count > 0)
			{
				target_eq_it.body->SetSocketAndStone(eq_socket_count,eq_stone_type);	
			}
		}
		if(inherit_type & PRODUCE_INHERIT_ENGRAVE)
		{
			//�̳д�����װ�����Կ�����
			if(eq_engrave_addon_count > 0)
			{
				target_eq_it.Engrave(&eq_engrave_addon_list[0], eq_engrave_addon_count);
			}
		
		}
		
		//�������������Ʒ����Ϣ
		_runner->produce_once(target_eq_it.type,rt.count - data->count,_inventory[rst].count, 0,rst);
	}

	if(data->count)
	{
		//ʣ������Ʒ
		DropItemData(_plane,_parent->pos,data,_parent->ID,0,0,0);
		//�����������Ҫ�ͷ��ڴ�
		*pItem = NULL;
		_runner->error_message(S2C::ERR_INVENTORY_IS_FULL);
		//���������ж�,�����ʵҲ�������ɹ���
		return true;
	}
	return true;
}

void gplayer_imp::PlayerRenameRet(const void *new_name, size_t name_len, int ret)
{
	__PRINTF("PlayerRenameRet namelen=%d ret=%d\n",name_len,ret);
	if (ret == 0)
	{
		//�޸��ڴ�����������
		SetPlayerName(new_name, name_len);
	}
}

// ����ʱ�����������ֵ
void gplayer_imp::UpdateMallConsumptionShopping(int id, unsigned int proc_type, int count, int total_price)
{
	if (world_manager::IsMallConsumptionItemShopping(id))
	{
		int tmp = _mall_consumption + total_price;
		if (tmp < _mall_consumption)
		{
			return;
		}
		_mall_consumption = tmp;
		
		char name_base64[MAX_USERNAME_LENGTH*2] = {0};
		base64_encode((unsigned char *)_username,(int)_username_len,name_base64);
		GLog::formatlog("mallconsumption:userid=%d:roleid=%d:rolename=%s:delta=%d:total=%d:type=%d:item=%d:item_count=%d",
				_db_user_id,_parent->ID.id,name_base64,total_price,_mall_consumption,0,id,count);
	}		

}

// ���˺�һʱ�����������ֵ
void gplayer_imp::UpdateMallConsumptionBinding(int id, unsigned int proc_type, int count)
{
	if (proc_type & item::ITEM_PROC_TYPE_CAN_WEBTRADE)
	{
		return;
	}

	int value;
	if (world_manager::GetMallConsumptionValueBinding(id, value))
	{
		float tmp1 = (float)value * (float)count;
		if (tmp1 > 2e9 || tmp1 < 0.f)
		{
			return;
		}

		int tmp2 = _mall_consumption + (value * count);
		if (tmp2 < _mall_consumption)
		{
			return;
		}
		_mall_consumption = tmp2;
		
		char name_base64[MAX_USERNAME_LENGTH*2] = {0};
		base64_encode((unsigned char *)_username,(int)_username_len,name_base64);
		GLog::formatlog("mallconsumption:userid=%d:roleid=%d:rolename=%s:delta=%d:total=%d:type=%d:item=%d:item_count=%d",
				_db_user_id,_parent->ID.id,name_base64,value*count,_mall_consumption,1,id,count);
	}
}

// ���Ļ�۳�ʱ�����������ֵ
void gplayer_imp::UpdateMallConsumptionDestroying(int id, unsigned int proc_type, int count)
{
	if (!(proc_type & item::ITEM_PROC_TYPE_MALL))
	{
		return;
	}

	int value;
	if (world_manager::GetMallConsumptionValueDestroying(id, value))
	{
		float tmp1 = (float)value * (float)count;
		if (tmp1 > 2e9 || tmp1 < 0.f)
		{
			return;
		}

		int tmp2 = _mall_consumption + (value * count);
		if (tmp2 < _mall_consumption)
		{
			return;
		}
		_mall_consumption = tmp2;
		
		char name_base64[MAX_USERNAME_LENGTH*2] = {0};
		base64_encode((unsigned char *)_username,(int)_username_len,name_base64);
		GLog::formatlog("mallconsumption:userid=%d:roleid=%d:rolename=%s:delta=%d:total=%d:type=%d:item=%d:item_count=%d",
				_db_user_id,_parent->ID.id,name_base64,value*count,_mall_consumption,2,id,count);
	}
}

int
gplayer_imp::PlayerEvolutionPet(size_t index,int formula_index)
{
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return S2C::ERR_PET_IS_NOT_EXIST;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;
	if(_petman.GetCurActivePet() == (int)index) return S2C::ERR_PET_IS_ALEARY_ACTIVE;
	if(pData->level < 100) return S2C::ERR_PET_CANNOT_EVOLUTION;
	//�����Ϲ�����
	if(formula_index > 2 || formula_index < 0)
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	DATA_TYPE dt;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pData->pet_tid, ID_SPACE_ESSENCE, dt);
	if(dt != DT_PET_ESSENCE || petess == NULL) return S2C::ERR_PET_CANNOT_EVOLUTION;
	
	PET_EVOLVE_CONFIG * pec = (PET_EVOLVE_CONFIG *)world_manager::GetDataMan().get_data_ptr(PET_EVOLVE_CONFIG_ID,ID_SPACE_CONFIG,dt);	
	if(dt != DT_PET_EVOLVE_CONFIG || pec == NULL) return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	int cost_index = petess->cost_index;
	int cost1 = pec->cost[cost_index].num_evolve[formula_index][0];
	int cost2 = pec->cost[cost_index].num_evolve[formula_index][1];
	
	if((cost1 && !_inventory.IsItemExist(PET_EVOLUTION_ITEM1,cost1)) ||(cost2 && !_inventory.IsItemExist(PET_EVOLUTION_ITEM2,cost2)))
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	//ȡ��������ĳ���id
	PET_EGG_ESSENCE * ess = (PET_EGG_ESSENCE *)world_manager::GetDataMan().get_data_ptr(petess->id_pet_egg_evolved,ID_SPACE_ESSENCE,dt);
	if(ess == NULL || dt != DT_PET_EGG_ESSENCE) return S2C::ERR_PET_CANNOT_EVOLUTION;

	int evolution_id = ess->id_pet; 
	
	//ȡ������������Ը��
	PET_ESSENCE * petess2 = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(evolution_id, ID_SPACE_ESSENCE, dt);
	if(dt != DT_PET_ESSENCE || petess2 == NULL) return S2C::ERR_PET_CANNOT_EVOLUTION;
	
	PET_EVOLVED_SKILL_RAND_CONFIG *pskill = (PET_EVOLVED_SKILL_RAND_CONFIG *)world_manager::GetDataMan().get_data_ptr(petess2->id_evolved_skill_rand,ID_SPACE_CONFIG,dt);
	if(dt != DT_PET_EVOLVED_SKILL_RAND_CONFIG || pskill == NULL) return S2C::ERR_PET_CANNOT_EVOLUTION;
	//�˴�ֱ��������Ը�
	int nature_group = abase::RandSelect(&(pskill->rand_skill_group[0].probability),sizeof(pskill->rand_skill_group[0]),3);
	int count = 0;
	unsigned int *pList = pskill->rand_skill_group[nature_group].list;
	for(int i=0;i<30;i++)
	{
		if(*(pList+i) == 0)
			continue;
		count ++;
	}
	int offset = abase::Rand(0,count-1);
	int pet_nature = pskill->rand_skill_group[nature_group].list[offset];

	//��ȡ����
	PET_EVOLVED_SKILL_CONFIG *pesc = (PET_EVOLVED_SKILL_CONFIG *)world_manager::GetDataMan().get_data_ptr(pet_nature,ID_SPACE_CONFIG,dt);
	if(dt != DT_PET_EVOLVED_SKILL_CONFIG || pesc == NULL) return S2C::ERR_PET_CANNOT_EVOLUTION;
	int skill1 = pesc->skills[0].id;
	int level1 = pesc->skills[0].level;
	int skill2 = pesc->skills[1].id;
	int level2 = pesc->skills[1].level;
	
	//����
	if(!_petman.EvolutionPet(this,index,evolution_id,pet_nature,skill1,level1,skill2,level2))
	{
		return S2C::ERR_PET_CANNOT_EVOLUTION;
	}
	//������Ʒ
	RemoveItems(PET_EVOLUTION_ITEM1,cost1, S2C::DROP_TYPE_USE, true);
	RemoveItems(PET_EVOLUTION_ITEM2,cost2, S2C::DROP_TYPE_USE, true);
	_runner->pet_evolution_done(index);
	return 0;
}

bool 
gplayer_imp::CalcPetEnhance2(const pet_data *pData, extend_prop &prop, int& attack_degree, int& defend_degree, int& vigour)
{
	prop.max_hp += (int)(((_base_prop.max_hp + player_template::GetVitHP(GetObjectClass(),_en_point.vit) + _en_point.max_hp) - 2400) * 0.01f * pData->evo_prop.r_hp);
	prop.damage_low += (int)((_cur_item.damage_magic_low + _en_point.magic_dmg_low + _base_prop.damage_magic_low) * 0.01f * pData->evo_prop.r_attack);
	prop.damage_high += (int)((_cur_item.damage_magic_high + _en_point.magic_dmg_high + _base_prop.damage_magic_high) * 0.01f * pData->evo_prop.r_attack);
	prop.defense += (int)((_base_prop.defense + _en_point.defense) * 0.01f * pData->evo_prop.r_defense);
	for(size_t i = 0; i < MAGIC_CLASS; i ++)
		prop.resistance[i] += (int)((_base_prop.resistance[i] + _en_point.resistance[i]) * 0.01f * pData->evo_prop.r_defense);
	attack_degree += (int)(_attack_degree * 0.01f * pData->evo_prop.r_atk_lvl);
	defend_degree += (int)(_defend_degree * 0.01f * pData->evo_prop.r_def_lvl);
	vigour = GetVigour();
	return true;	
}	

int 
gplayer_imp::PlayerAddPetExp(size_t index,size_t num)
{
	//�������Ƿ�����Լ��Ƿ��ܱ�ι��
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return S2C::ERR_PET_IS_NOT_EXIST;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;
	if((pData->pet_class != pet_data::PET_CLASS_COMBAT && pData->pet_class != pet_data::PET_CLASS_EVOLUTION) || num == 0 || _petman.GetCurActivePet() != (int)index )
	{
		return S2C::ERR_PET_TYPE_WRONG;
	}
	//�����Ʒ�Ƿ��㹻
	if(!_inventory.IsItemExist(PET_ADDEXP_ITEM,num))
	{
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	}
	//ι��
	int exp_list[5] = {500,1250,3125,7812,19531};
	int cost_num = 0;
	for(size_t i=0;i < num; i++)
	{
		short pet_level = pData->level;
		int exp_gain = 0;
		if(pet_level <=100)
		{
			exp_gain = exp_list[0];
		}
		else if(pet_level <=104)
		{
			exp_gain = exp_list[pet_level - 100];
		}
		else
		{
			exp_gain = exp_list[4];
		}
		if(_petman.AddExp(this,index,exp_gain))
		{
			cost_num ++;
		}
		else
		{
			break;
		}
	}
	//�۳����p
	if(cost_num > 0)
	{	
		RemoveItems(PET_ADDEXP_ITEM,cost_num,S2C::DROP_TYPE_USE,true);
	}
	return 0;
}

int
gplayer_imp::PlayerRebuildPetNature(size_t index,int formula_index)
{
	//�������Ƿ�Ϸ�
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return S2C::ERR_PET_IS_NOT_EXIST;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;
	if(_petman.GetCurActivePet() == (int)index) return S2C::ERR_PET_IS_ALEARY_ACTIVE;
	if(pData->pet_class != pet_data::PET_CLASS_EVOLUTION)
	{
		return S2C::ERR_PET_TYPE_WRONG;
	}
	//����ײͲ����Ƿ����
	DATA_TYPE dt;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pData->pet_tid, ID_SPACE_ESSENCE, dt);
	if(petess == NULL || dt != DT_PET_ESSENCE)	return S2C::ERR_ITEM_NOT_IN_INVENTORY;	
	PET_EVOLVE_CONFIG * pec = (PET_EVOLVE_CONFIG *)world_manager::GetDataMan().get_data_ptr(PET_EVOLVE_CONFIG_ID,ID_SPACE_CONFIG,dt);	
	if(dt != DT_PET_EVOLVE_CONFIG || pec == NULL) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(formula_index < 0 || formula_index > 2) return S2C::ERR_ITEM_NOT_IN_INVENTORY; 
	int cost_index = petess->cost_index;
	int cost1 = pec->cost[cost_index].num_rand_skill[formula_index][0];
	int cost2 = pec->cost[cost_index].num_rand_skill[formula_index][1];
	if((cost1 && !_inventory.IsItemExist(PET_EVOLUTION_ITEM1,cost1)) ||(cost2 && !_inventory.IsItemExist(PET_EVOLUTION_ITEM2,cost2)))
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(HasSession())
		return S2C::ERR_CANNOT_REBUILD;

	session_rebuild_pet_nature *pSession = new session_rebuild_pet_nature(this,pData->pet_tid,index,formula_index);
	if(AddSession(pSession)) StartSession();
	return 0;
}

int
gplayer_imp::PlayerRebuildPetInheritRatio(size_t index,int formula_index)
{
	//�������Ƿ�Ϸ�
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return S2C::ERR_PET_IS_NOT_EXIST;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData) return S2C::ERR_PET_IS_NOT_EXIST;
	if(_petman.GetCurActivePet() == (int)index) return S2C::ERR_PET_IS_ALEARY_ACTIVE;
	if(pData->pet_class != pet_data::PET_CLASS_EVOLUTION)
	{
		return S2C::ERR_PET_TYPE_WRONG;
	}
	//�������ײ��Ƿ����
	DATA_TYPE dt;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pData->pet_tid, ID_SPACE_ESSENCE, dt);
	if(petess == NULL || dt != DT_PET_ESSENCE)	return S2C::ERR_ITEM_NOT_IN_INVENTORY;	
	PET_EVOLVE_CONFIG * pec = (PET_EVOLVE_CONFIG *)world_manager::GetDataMan().get_data_ptr(PET_EVOLVE_CONFIG_ID,ID_SPACE_CONFIG,dt);	
	if(dt != DT_PET_EVOLVE_CONFIG || pec == NULL) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(formula_index < 0 || formula_index > 2) return S2C::ERR_ITEM_NOT_IN_INVENTORY; 
	int cost_index = petess->cost_index;
	int cost1 = pec->cost[cost_index].num_inherit[formula_index][0];
	int cost2 = pec->cost[cost_index].num_inherit[formula_index][1];
	if((cost1 && !_inventory.IsItemExist(PET_EVOLUTION_ITEM1,cost1)) || (cost2 && !_inventory.IsItemExist(PET_EVOLUTION_ITEM2,cost2)))
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(HasSession())
		return S2C::ERR_CANNOT_REBUILD;
	
	session_rebuild_pet_inheritratio *pSession = new session_rebuild_pet_inheritratio(this,pData->pet_tid,index,formula_index);
	if(AddSession(pSession)) StartSession();
	return 0;
}

bool 
gplayer_imp::RebulidPetInheritRatio(int pet_id,size_t index,int formula_index,int &r_attack,int &r_defense,int &r_hp,int &r_atk_lvl,int &r_def_lvl)
{
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return false;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData || pData->pet_tid != pet_id) return false;
	if(_petman.GetCurActivePet() == (int)index) return false;
	if(pData->pet_class != pet_data::PET_CLASS_EVOLUTION)
	{
		return false;
	}
	if(!_petman.RebuildInheritRatio(pet_id,r_attack,r_defense,r_hp,r_atk_lvl,r_def_lvl))
	{
		return false;
	}
	//���Ͽ۳�
	DATA_TYPE dt;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pet_id, ID_SPACE_ESSENCE, dt);
	if(petess == NULL || dt != DT_PET_ESSENCE)	return false;	
	PET_EVOLVE_CONFIG * pec = (PET_EVOLVE_CONFIG *)world_manager::GetDataMan().get_data_ptr(PET_EVOLVE_CONFIG_ID,ID_SPACE_CONFIG,dt);	
	if(dt != DT_PET_EVOLVE_CONFIG || pec == NULL) return false;
	int cost_index = petess->cost_index;
	int cost1 = pec->cost[cost_index].num_inherit[formula_index][0];
	int cost2 = pec->cost[cost_index].num_inherit[formula_index][1];
	RemoveItems(PET_EVOLUTION_ITEM1,cost1, S2C::DROP_TYPE_USE, true);
	RemoveItems(PET_EVOLUTION_ITEM2,cost2, S2C::DROP_TYPE_USE, true);
	//֪ͨ�ͻ���
	_runner->pet_rebuild_inherit_info((int)g_timer.get_systime() + 30,pet_id,index,r_attack,r_defense,r_hp,r_atk_lvl,r_def_lvl);
	return true;
}

void 
gplayer_imp::PlayerAcceptRebuildInheritResult(bool isaccept)
{
	if (!_cur_session || _cur_session->GetRunTimeClass()->GetGUID() != CLS_SESSION_REBUILD_PET_INHERITRATIO || _cur_session->_session_id < 0)
	{
		return;
	}

	((session_rebuild_pet_inheritratio*)_cur_session)->AcceptResult(isaccept);
}

void
gplayer_imp::AcceptInheritRatioResult(int pet_id,size_t index,int r_attack,int r_defense,int r_hp,int r_atk_lvl,int r_def_lvl)
{
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData || pData->pet_tid != pet_id) return;
	if(_petman.GetCurActivePet() == (int)index) return;
	if(pData->pet_class != pet_data::PET_CLASS_EVOLUTION) return;

	_petman.PetAcceptInheritRatioResult(this,index,r_attack,r_defense,r_hp,r_atk_lvl,r_def_lvl);
}

bool
gplayer_imp::RebuildPetNature(int pet_id,size_t index,int formula_index,int &nature)
{
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return false;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData || pData->pet_tid != pet_id) return false;
	if(_petman.GetCurActivePet() == (int)index) return false;
	if(pData->pet_class != pet_data::PET_CLASS_EVOLUTION)
	{
		return false;
	}
	//�ٴ�����Ը�
	DATA_TYPE dt;
	PET_EVOLVE_CONFIG * pec = (PET_EVOLVE_CONFIG *)world_manager::GetDataMan().get_data_ptr(PET_EVOLVE_CONFIG_ID,ID_SPACE_CONFIG,dt);	
	if(dt != DT_PET_EVOLVE_CONFIG || pec == NULL) return false;
	PET_ESSENCE * petess = (PET_ESSENCE *)world_manager::GetDataMan().get_data_ptr(pData->pet_tid, ID_SPACE_ESSENCE, dt);
	if(dt != DT_PET_ESSENCE || petess == NULL) return false;
	PET_EVOLVED_SKILL_RAND_CONFIG *pskill = (PET_EVOLVED_SKILL_RAND_CONFIG *)world_manager::GetDataMan().get_data_ptr(petess->id_evolved_skill_rand,ID_SPACE_CONFIG,dt);
	if(dt != DT_PET_EVOLVED_SKILL_RAND_CONFIG || pskill == NULL) return false;
	int nature_group = abase::RandSelect(&(pskill->rand_skill_group[0].probability),sizeof(pskill->rand_skill_group[0]),3);
	int count = 0;
	unsigned int *pList = pskill->rand_skill_group[nature_group].list;
	for(int i=0;i<30;i++)
	{
		if(*(pList+i) == 0)
			continue;
		count ++;
	}
	int offset = abase::Rand(0,count-1);
	nature = pskill->rand_skill_group[nature_group].list[offset];
	//�۳�����
	int cost_index = petess->cost_index;
	int cost1 = pec->cost[cost_index].num_rand_skill[formula_index][0];
	int cost2 = pec->cost[cost_index].num_rand_skill[formula_index][1];
	RemoveItems(PET_EVOLUTION_ITEM1,cost1, S2C::DROP_TYPE_USE, true);
	RemoveItems(PET_EVOLUTION_ITEM2,cost2, S2C::DROP_TYPE_USE, true);
	//֪ͨ�ͻ���
	_runner->pet_rebuild_nature_info((int)g_timer.get_systime()+30,pet_id,index,nature);
	return true;
}
void 
gplayer_imp::PlayerAcceptRebuildNatureResult(bool isaccept)
{
	if (!_cur_session || _cur_session->GetRunTimeClass()->GetGUID() != CLS_SESSION_REBUILD_PET_NATURE || _cur_session->_session_id < 0)
	{
		return;
	}

	((session_rebuild_pet_nature*)_cur_session)->AcceptResult(isaccept);
}

void
gplayer_imp::PlayerAskForPresent(int roleid, int goods_id, int goods_index, int goods_slot)
{
	if (roleid <= 0) return;
	
	int self_id = GetParent()->ID.id;
	if (self_id == roleid) return;
	
	if(_player_state != PLAYER_STATE_NORMAL) return;

	netgame::mall & shop = world_manager::GetPlayerMall();
	int __group_id = shop.GetGroupId();
	time_t __time = time(NULL);
	
	ASSERT(netgame::mall::MAX_ENTRY == 4);
	if(goods_slot >= netgame::mall::MAX_ENTRY)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}
	netgame::mall::node_t node;
	if(!shop.QueryGoods(goods_index,node) || node.goods_id != goods_id)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}

	if(!node.check_owner(0))
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}

	if(node.entry[goods_slot].cash_need <= 0)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}
	if(IsItemForbidShop(node.goods_id)
			|| node.gift_id > 0 && IsItemForbidShop(node.gift_id))
	{
		_runner->error_message(S2C::ERR_ITEM_FORBID_SHOP);
		return;
	}
	//�ҵ���ǰ��Ч��group
	int active_group_id = 0;
	if(node.group_active && __group_id != 0)
	{
		if(__group_id == node.entry[0].group_id || __group_id == node.entry[1].group_id || __group_id == node.entry[2].group_id || __group_id == node.entry[3].group_id)
			active_group_id = __group_id;	
	}

	if(node.sale_time_active)
	{	
		if(node.entry[goods_slot].group_id == active_group_id && node.entry[goods_slot]._sale_time.CheckAvailable(__time))	
		{
			//���playerѡ���slot�����õ����۷�ʽ������Ҫɨ�赱ǰ��Ч���ڣ����Ƿ񻹴��ڷ��������۷�ʽ�����
			if(node.entry[goods_slot]._sale_time.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
			{
				for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
				{
					if(node.entry[j].cash_need <= 0)
						break;
					if(node.entry[j].group_id == active_group_id 
								&& node.entry[j]._sale_time.GetType() != netgame::mall::sale_time::TYPE_NOLIMIT
								&& node.entry[j]._sale_time.CheckAvailable(__time))
					{
						_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
						return;
					}
				}
			}				
		}
		else
		{
			_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
			return;
		}
	}
	else if(node.entry[goods_slot].group_id != active_group_id)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}

	GMSV::SendPlayerAskForPresent(self_id, roleid, goods_id, goods_index, goods_slot);

	GLog::log(GLOG_INFO,"�û�%d��ʼ�ڰٱ�����%d��ȡ��Ʒ%d",self_id,roleid,goods_id);
}

void
gplayer_imp::PlayerGivePresent(int roleid, int mail_id, int goods_id, int goods_index, int goods_slot)
{
	if (roleid <= 0 || mail_id < -1) return;
	
	int self_id = GetParent()->ID.id;
	if (self_id == roleid) return;
	
	if(_player_state != PLAYER_STATE_NORMAL) return;

	if (OI_TestSafeLock())
	{
		_runner->error_message(S2C::ERR_FORBIDDED_OPERATION_IN_SAFE_LOCK);
		return;
	}

	netgame::mall & shop = world_manager::GetPlayerMall();
	int __group_id = shop.GetGroupId();
	time_t __time = time(NULL);
	netgame::mall_order  order(_mall_order_id);
	
	ASSERT(netgame::mall::MAX_ENTRY == 4);
	if(goods_slot >= netgame::mall::MAX_ENTRY)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}
	netgame::mall::node_t node;
	if(!shop.QueryGoods(goods_index,node) || node.goods_id != goods_id)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}

	if(!node.check_owner(0))
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}

	if(node.entry[goods_slot].cash_need <= 0)
	{
		_runner->error_message(S2C::ERR_GSHOP_INVALID_REQUEST);
		return;
	}
	if(IsItemForbidShop(node.goods_id)
			|| node.gift_id > 0 && IsItemForbidShop(node.gift_id))
	{
		_runner->error_message(S2C::ERR_ITEM_FORBID_SHOP);
		return;
	}
	//�ҵ���ǰ��Ч��group
	int active_group_id = 0;
	if(node.group_active && __group_id != 0)
	{
		if(__group_id == node.entry[0].group_id || __group_id == node.entry[1].group_id || __group_id == node.entry[2].group_id || __group_id == node.entry[3].group_id)
			active_group_id = __group_id;	
	}

	if(node.sale_time_active)
	{	
		if(node.entry[goods_slot].group_id == active_group_id && node.entry[goods_slot]._sale_time.CheckAvailable(__time))	
		{
			//���playerѡ���slot�����õ����۷�ʽ������Ҫɨ�赱ǰ��Ч���ڣ����Ƿ񻹴��ڷ��������۷�ʽ�����
			if(node.entry[goods_slot]._sale_time.GetType() == netgame::mall::sale_time::TYPE_NOLIMIT)
			{
				for(int j=0; j<netgame::mall::MAX_ENTRY; j++)
				{
					if(node.entry[j].cash_need <= 0)
						break;
					if(node.entry[j].group_id == active_group_id 
								&& node.entry[j]._sale_time.GetType() != netgame::mall::sale_time::TYPE_NOLIMIT
								&& node.entry[j]._sale_time.CheckAvailable(__time))
					{
						_runner->mall_item_buy_failed(goods_index, 0);
						return;
					}
				}
			}				
		}
		else
		{
			_runner->mall_item_buy_failed(goods_index, 0);
			return;
		}
	}
	else if(node.entry[goods_slot].group_id != active_group_id)
	{
		_runner->mall_item_buy_failed(goods_index, 0);
		return;
	}

	//���ӵ�order���ȥ����ôд��Ϊ�˸�ʽ�Ϻ��̳ǹ��򱣳�һ��
	order.AddGoods(node.goods_id, node.goods_count,node.entry[goods_slot].cash_need, node.entry[goods_slot].expire_time,node.entry[goods_slot].expire_type,node.gift_id,node.gift_count,node.gift_expire_time,node.gift_log_price);

	if(GetMallCash() < order.GetPointRequire())
	{
		//no engouh mall cash 
		_runner->error_message(S2C::ERR_OUT_OF_FUND);
		return;
	}
	
	unsigned int cash_used = 0;
	//��Ǯ�㹻�� ��ʼ����
	int cur_t = g_timer.get_systime();

	int id;
	int count;
	int point;
	int expire_time;
	int expire_type;
	int gift_id;
	int gift_count;
	int gift_expire_time;
	int gift_log_price;
	int log_price1;
	int log_price2;
	item new_item;
	item new_gift;
	bool has_gift = false;
	bool bRst = order.GetGoods(0, id, count,point, expire_time,expire_type,gift_id,gift_count,gift_expire_time,gift_log_price);
	if(bRst)
	{
		//������Ʒ����Ʒ��log�۸�
		log_price1 = point;
		log_price2 = 0;
		if(gift_id > 0 && gift_log_price > 0)
		{
			log_price1 = int((float)point*point/(point+gift_log_price));
			log_price2 = point - log_price1; 
		}
		
		const item_data * pItem = (const item_data*)world_manager::GetDataMan().get_item_for_sell(id);
		if(pItem)
		{
			item_data * pItem2 = DupeItem(*pItem);
			int expire_date = 0;
			if(expire_time) 
			{
				if(expire_type == netgame::mall::EXPIRE_TYPE_TIME)
				{
					//��Ч������һ������
					expire_date = cur_t + expire_time;
				}
				else
				{
					//��Ч���ǹ涨����ʧЧ
					expire_date = expire_time;
				}
			}
			int guid1 = 0;
			int guid2 = 0;
			if(pItem2->guid.guid1 != 0)
			{
				get_item_guid(pItem2->type, guid1,guid2);
				pItem2->guid.guid1 = guid1;
				pItem2->guid.guid2 = guid2;
			}

			pItem2->proc_type |= item::ITEM_PROC_TYPE_MALL;
			pItem2->count = count;
			pItem2->expire_date = expire_date;

			//����item����,��Ҫ�ͷ�new_item
			if(!MakeItemEntry(new_item,*pItem2))
			{
				FreeItem(pItem2);
				//��¼������־
				GLog::log(GLOG_ERR,"�û�%d�ڹ���ٱ�����Ʒ%d����%dʱ������ʱ��Ʒʧ��",self_id, id, roleid);
				return;
			}
			
			//�������³�ʼ��һ�¿��ܵ�ʱװ
            new_item.InitFromShop();

			cash_used += (unsigned int)log_price1;

			FreeItem(pItem2);
		}
		else
		{
			//��¼������־
			GLog::log(GLOG_ERR,"�û�%d�ڹ���ٱ�����Ʒ%d����%dʱ������Ʒʧ��",self_id, id, roleid);
			return;
		}

		//����Ϊ������Ʒ
		if(gift_id > 0)
		{
			const item_data * pGift = (const item_data*)world_manager::GetDataMan().get_item_for_sell(gift_id);
			if(pGift)
			{
				has_gift = true;
				item_data * pGift2 = DupeItem(*pGift);
				int expire_date = 0;
				if(gift_expire_time) 
				{
					//��Ч������һ������
					expire_date = cur_t + gift_expire_time;
				}
				int guid1 = 0;
				int guid2 = 0;
				if(pGift2->guid.guid1 != 0)
				{
					get_item_guid(pGift2->type, guid1,guid2);
					pGift2->guid.guid1 = guid1;
					pGift2->guid.guid2 = guid2;
				}
				
				pGift2->count = gift_count;
				pGift2->expire_date = expire_date;

				//����item����,��Ҫ�ͷ�new_gift
				if(!MakeItemEntry(new_gift,*pGift2))
				{
					//�ͷ����ɵ���Ʒ
					new_item.Release();
					FreeItem(pGift2);
					//��¼������־
					GLog::log(GLOG_ERR,"�û�%d�ڹ���ٱ�����Ʒ%d����%dʱ������ʱ��Ʒʧ��",self_id, id, roleid);
					return;
				}

				//�������³�ʼ��һ�¿��ܵ�ʱװ
                new_gift.InitFromShop();
				
				cash_used += (unsigned int)log_price2;
				
				FreeItem(pGift2);
			}
			else
			{
				//�ͷ����ɵ���Ʒ
				new_item.Release();
				//��¼������־
				GLog::log(GLOG_ERR,"�û�%d�ڹ���ٱ�����Ʒ%d����%dʱ������Ʒ%dʧ��",self_id, goods_id, roleid,gift_id);
				return;
			}
		}
	}
	else
	{
		ASSERT(false);
	}

	GDB::itemlist list;
	if (!has_gift)
	{
		list.list = (GDB::itemdata*)abase::fast_allocator::alloc(sizeof(GDB::itemdata));
		list.count = 1;

		GDB::itemdata * pData = list.list;
		item_list::ItemToData(new_item, pData[0]);
	}
	else
	{
		list.list = (GDB::itemdata*)abase::fast_allocator::alloc(2*sizeof(GDB::itemdata));
		list.count = 2;

		GDB::itemdata * pData = list.list;
		item_list::ItemToData(new_item, pData[0]);
		item_list::ItemToData(new_gift, pData[1]);
	}

	//������Ϣ��gdeliveryd
	object_interface oi(this);
	GMSV::SendPlayerGivePresent(self_id, roleid, cash_used, has_gift, log_price1, log_price2, mail_id, list, oi);

	//�ͷ����ɵ���Ʒ
	new_item.Release();
	new_gift.Release();
	//�ͷ�itemlist
	GetInventory().ReleaseDBData(list);

	GLog::log(GLOG_INFO,"�û�%d��ʼ�ڰٱ�����%d��Ʒ����%d��Ԥ�ƻ���%d��ʣ��%d��",self_id,goods_id,roleid,cash_used,GetMallCash()-(int)cash_used);
}

void
gplayer_imp::AcceptNatureResult(int pet_id,size_t index,int nature)
{
	if(index >= pet_manager::MAX_PET_CAPACITY)
		return;

	pet_data * pData = _petman.GetPetData(index);
	if(!pData || pData->pet_tid != pet_id) return;
	if(_petman.GetCurActivePet() == (int)index) return;
	if(pData->pet_class != pet_data::PET_CLASS_EVOLUTION) return;

	//�����µ��Ը��ȡ����
	DATA_TYPE dt;
	PET_EVOLVED_SKILL_CONFIG *pesc = (PET_EVOLVED_SKILL_CONFIG *)world_manager::GetDataMan().get_data_ptr(nature,ID_SPACE_CONFIG,dt);
	if(dt != DT_PET_EVOLVED_SKILL_CONFIG || pesc == NULL) return;

	_petman.PetAcceptNatureResult(this,index,nature, pesc->skills[0].id,pesc->skills[0].level,pesc->skills[1].id,pesc->skills[1].level);
}

void 
gplayer_imp::GetNatureSkill(int nature,int &skill1,int &skill2)
{
	if(!nature) return;
	DATA_TYPE dt;
	PET_EVOLVED_SKILL_CONFIG *pesc = (PET_EVOLVED_SKILL_CONFIG *)world_manager::GetDataMan().get_data_ptr(nature,ID_SPACE_CONFIG,dt);
	if(dt != DT_PET_EVOLVED_SKILL_CONFIG || pesc == NULL) return;
	
	skill1 = pesc->skills[0].id;
	skill2 = pesc->skills[1].id;
}

bool 
gplayer_imp::ChangeEquipAddon(unsigned char equip_idx,unsigned char equip_socket_idx,int old_stone_type,int new_stone_type,int recipe_type,int* materials_ids,unsigned char* idxs,int count)
{
	const addonchange_recipe_template * acrt = recipe_manager::GetAddonChangeRecipe(recipe_type);
	
	if(!acrt) return false; // before onserve already check

    if (world_manager::GetGlobalController().CheckServerForbid(SERVER_FORBID_RECIPE, acrt->recipe_id)) return false;

	int ret = 0;
	item_list & inv = GetInventory();
	size_t inv_size = inv.Size();

	if(equip_idx >= inv_size || inv[equip_idx].type <= 0 
	   || !inv[equip_idx].HasAddonAtSocket(equip_socket_idx,old_stone_type))// HasAddonAtSocket ˳���ж��� inv[equip_idx].body != NULL 
	{
		//У�����Ʒ�Ƿ����
		ret = S2C::ERR_NO_EQUAL_EQUIPMENT_FAIL;
	}
	else if(GetMoney() < acrt->fee)
	{
		ret = S2C::ERR_ENOUGH_MONEY_IN_TRASH_BOX;
	}
	else
	{
		std::set<unsigned char> unique_idx;
		for(int idx = 0; idx < count; ++idx)
		{
			if(0 == acrt->material_list[idx].item_id) continue;

			if(!inv.IsItemExist(idxs[idx], acrt->material_list[idx].item_id , acrt->material_list[idx].count )
					|| !unique_idx.insert(idx).second ) // �Ѿ�����materials_id[idx] == ess->materials[idx]
			{
				ret = S2C::ERR_NO_EQUAL_RECIPE_FAIL;
				break;
			}
		}

		if(ret == 0)
		{
			//�ж��Ƿ�ʯ�ͱ�ʯ�����Ƿ���ȷ
			DATA_TYPE dt;
			STONE_ESSENCE * st_ess = (STONE_ESSENCE *) world_manager::GetDataMan().get_data_ptr(new_stone_type,ID_SPACE_ESSENCE,dt);
			if(dt != DT_STONE_ESSENCE || ! st_ess) 
			{
				ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
			}
			else
			{
				const void * pess = world_manager::GetDataMan().get_data_ptr(inv[equip_idx].type,ID_SPACE_ESSENCE,dt);
				if(pess) 
				{
					switch(dt)
					{
						case DT_WEAPON_ESSENCE:
						if(st_ess->level > ((WEAPON_ESSENCE*)pess)->level) 
							ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
						break;
						case DT_ARMOR_ESSENCE:
						if(st_ess->level > ((ARMOR_ESSENCE*)pess)->level) 
							ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
						break;
						default:	
							ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
					}
				}
				else
				{
					ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
				}
			}
		}
	}

	if(ret > 0)	//У���ɿͻ��˴��ݵĲ������ִ���
	{
		_runner->error_message(ret);
		GLog::log(GLOG_ERR, "�û�[%d]��ʯת�������ִ���%d",GetParent()->ID.id,ret);
		return false;
	}
	
	if(false == inv[equip_idx].ModifyAddonAtSocket(equip_socket_idx,new_stone_type))
	{
		_runner->error_message(S2C::ERR_MODIFY_ADDON_FAIL);
		GLog::log(GLOG_ERR, "�û�[%d]��ʯת��ִ�г��ִ���",GetParent()->ID.id);
		return false;	
	}

	//֪ͨ�ͻ������ݸ���
	PlayerGetItemInfo(IL_INVENTORY,equip_idx);

        //�۳���Ǯ
	if(acrt->fee)
	{
		SpendMoney(acrt->fee);
		_runner->spend_money(acrt->fee);
	}
		
	//�۳�����
	for(int idx = 0; idx < count; ++idx)
	{
		if(0 == acrt->material_list[idx].item_id) continue;

		item& it = inv[idxs[idx]];
		UpdateMallConsumptionDestroying(it.type, it.proc_type, acrt->material_list[idx].count);
			
		inv.DecAmount(idxs[idx],acrt->material_list[idx].count);
		_runner->player_drop_item(gplayer_imp::IL_INVENTORY,idxs[idx], acrt->material_list[idx].item_id,acrt->material_list[idx].count,S2C::DROP_TYPE_PRODUCE);
	}

	//֪ͨ�ͻ��˳ɹ�ת��
	_runner->equip_addon_update_notify(0,equip_idx,equip_socket_idx,old_stone_type,new_stone_type);

	GLog::log(GLOG_INFO,"�û�%d��װ��%d��%dʹ���䷽%d�����ɱ�ʯ%d���±�ʯ%d",
		GetParent()->ID.id,inv[equip_idx].type,equip_socket_idx,recipe_type,old_stone_type,new_stone_type);

	return true;
}

bool 
gplayer_imp::ReplaceEquipAddon(unsigned char equip_idx,unsigned char equip_socket_idx,int old_stone_type,int new_stone_type,unsigned char new_stone_idx)
{
	item_list & inv = GetInventory();
	size_t inv_size = inv.Size();

	int ret = 0, total_cost = 0;

	if(equip_idx >= inv_size || inv[equip_idx].type <= 0 || !inv[equip_idx].HasAddonAtSocket(equip_socket_idx,old_stone_type)) 
	{
		//У�����Ʒ�Ƿ����
		ret = S2C::ERR_NO_EQUAL_EQUIPMENT_FAIL;
	}
	else if(new_stone_idx >= inv_size || inv[new_stone_idx].type != new_stone_type)
	{
		ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
	}
	else
	{
		//�ж��Ƿ�ʯ�ͱ�ʯ�����Ƿ���ȷ
		DATA_TYPE dt_old ,dt_new, dt_eq;
		STONE_ESSENCE * ess_old = (STONE_ESSENCE *)world_manager::GetDataMan().get_data_ptr(old_stone_type, ID_SPACE_ESSENCE, dt_old);
		STONE_ESSENCE * ess_new = (STONE_ESSENCE *)world_manager::GetDataMan().get_data_ptr(new_stone_type, ID_SPACE_ESSENCE, dt_new);
		if(dt_old != DT_STONE_ESSENCE || dt_new != DT_STONE_ESSENCE ||! ess_old ||! ess_new) 
		{
			ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
		}
		else if((unsigned int)(total_cost = ess_old->uninstall_price + ess_new->install_price) > GetMoney() || total_cost < 0)
		{
			ret = S2C::ERR_OUT_OF_FUND;
		}
		else
		{
			const void * pess = world_manager::GetDataMan().get_data_ptr(inv[equip_idx].type,ID_SPACE_ESSENCE,dt_eq);
			if(pess) 
			{
				switch(dt_eq)
				{
					case DT_WEAPON_ESSENCE:
					if(ess_new->level > ((WEAPON_ESSENCE*)pess)->level) 
						ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
					break;
					case DT_ARMOR_ESSENCE:
					if(ess_new->level > ((ARMOR_ESSENCE*)pess)->level) 
						ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
					break;
					default:	
						ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
				}
			}			
			else
			{
				ret = S2C::ERR_NO_EQUAL_DEST_FAIL;
			}
		}
	}


	if(ret > 0)	//У���ɿͻ��˴��ݵĲ������ִ���

	{
		_runner->error_message(ret);
		GLog::log(GLOG_ERR, "�û�[%d]��ʯ�滻�����ִ���%d",GetParent()->ID.id,ret);
		return false;
	}

	if(false == inv[equip_idx].ModifyAddonAtSocket(equip_socket_idx,new_stone_type))
	{
		_runner->error_message(S2C::ERR_MODIFY_ADDON_FAIL);
		GLog::log(GLOG_ERR, "�û�[%d]��ʯ�滻ִ�г��ִ���",GetParent()->ID.id);
		return false;	
	}
	
	//֪ͨ�ͻ������ݸ���
	PlayerGetItemInfo(IL_INVENTORY,equip_idx);
	
	//�۳��滻�û�ʯ
	item& it = inv[new_stone_idx];
	UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
			
	inv.DecAmount(new_stone_idx,1);
	_runner->player_drop_item(gplayer_imp::IL_INVENTORY,new_stone_idx, new_stone_type, 1,S2C::DROP_TYPE_PRODUCE);

	// �۳��滻�ý�Ǯ
	SpendMoney(total_cost);
	_runner->spend_money(total_cost);

	//֪ͨ�ͻ��˳ɹ��滻
	_runner->equip_addon_update_notify(1,equip_idx,equip_socket_idx,old_stone_type,new_stone_type);

	GLog::log(GLOG_INFO,"�û�%d��װ��%d��%dʹ���±�ʯ%d�滻�ɱ�ʯ%d",
		GetParent()->ID.id,inv[equip_idx].type,equip_socket_idx,new_stone_type,old_stone_type);

	return true;

}

int
gplayer_imp::PlayerTryRefineMeridian(int index)
{
	//������ŵ�index�Ƿ�Ϸ� 0-47
	if(index < 0 || index >= MERIDIAN_TRIGRAMS_SIZE) return S2C::ERR_TRY_REFINE_MERIDIAN_FAIL;
	//�жϵȼ��Ƿ�Ϸ�
	if(!_meridianman.CheckMeridianCondition(GetHistoricalMaxLevel())) return S2C::ERR_TRY_REFINE_MERIDIAN_FAIL;
	int free_cost_id = MERIDIAN_REFINE_COST1;
	int free_cost_index = _inventory.Find(0,MERIDIAN_REFINE_COST1);
	//�����ѳ�������Ƿ��㹻
	if(free_cost_index < 0)
	{
		free_cost_id = MERIDIAN_REFINE_COST2;
		free_cost_index = _inventory.Find(0,MERIDIAN_REFINE_COST2);
		if(free_cost_index < 0)
		{
			free_cost_id = MERIDIAN_REFINE_COST3;
			free_cost_index = _inventory.Find(0,MERIDIAN_REFINE_COST3);
		}
	}
	if(free_cost_index < 0)
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	
	bool have_free_times = _meridianman.CheckMeridianFreeRefineTimes();
	int paid_cost_id = MERIDIAN_PAID_REFINE_COST1;
	//���û����ѳ��������Ҫ����շѳ������
	if(!have_free_times)
	{
		int paid_cost_index = _inventory.Find(0,MERIDIAN_PAID_REFINE_COST1);
		if(paid_cost_index < 0)
		{
			paid_cost_id = MERIDIAN_PAID_REFINE_COST2;
			paid_cost_index = _inventory.Find(0,MERIDIAN_PAID_REFINE_COST2);
			if(paid_cost_index < 0)
			{
				paid_cost_id = MERIDIAN_PAID_REFINE_COST3;
				paid_cost_index = _inventory.Find(0,MERIDIAN_PAID_REFINE_COST3);
				if(paid_cost_index < 0)
				{
					paid_cost_id = MERIDIAN_PAID_REFINE_COST4;
					paid_cost_index = _inventory.Find(0,MERIDIAN_PAID_REFINE_COST4);
				}
			}
		}
		if(paid_cost_index < 0)
			return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	}
	//���г��	
	_meridianman.Deactivate(this);	
	int rst = _meridianman.TryRefineMeridian(this,index);
	if(rst == meridian_manager::MERIDIAN_FATAL_ERR)
	{
		_meridianman.Activate(this);
		return S2C::ERR_TRY_REFINE_MERIDIAN_FAIL;
	}
	_meridianman.Activate(this);
	//����һ�¾�������
	if(rst == meridian_manager::MERIDIAN_LIFE_REFINE)
	{
		property_policy::UpdatePlayer(GetPlayerClass(),this);
	}
	//�۳�����
	RemoveItems(free_cost_id,1,S2C::DROP_TYPE_USE, true);
	if(!have_free_times)
	{
		RemoveItems(paid_cost_id,1,S2C::DROP_TYPE_USE, true);
	}
	//֪ͨ�ͻ���
	_runner->try_refine_meridian_result(index,rst);	
	return 0;
}

void
gplayer_imp::PlayerAddFreeRefineTimes(int num)
{
	_meridianman.AddFreeRefineTime(num);
	_meridianman.NotifyMeridianData(this);
}

void 
gplayer_imp::OnTouchPointQuery(int64_t income,int64_t remain)
{
	_touch_order.income = income;
	_touch_order.remain = remain;

	_runner->notify_touch_query(income,remain,0);
}

void 
gplayer_imp::OnTouchPointCost(int64_t orderid,unsigned int cost,int64_t income,int64_t remain,int retcode)
{
	_touch_order.income = income;
	_touch_order.remain = remain;

	if(!_touch_order.IsHalfTrade(orderid))
	{
		GLog::log(LOG_ERR,"roleid:%d cost point callback fail [orderid%lld cost%d income%lld remain%lld retcode%d]",_parent->ID.id,orderid,cost,income,remain,retcode);
		return;
	}

	GLog::log(LOG_INFO,"roleid:%d cost point orderid%lld cost%d income%lld remain%lld retcode%d",_parent->ID.id,orderid,cost,income,remain,retcode);

	// log
	switch(retcode)
	{
		case touch_trade::TPC_SUCCESS:
		case touch_trade::TPC_COMPLETE:
			_touch_order.Complete(this);
			break;
		case touch_trade::TPC_BUSY:
			GLog::log(LOG_INFO,"roleid:%d touch %lld cost point busy",_parent->ID.id,orderid);
			break;
		case touch_trade::TPC_ORDER_CLASH:
			_touch_order.OnIdClash(this);
			break;
		default:
			_touch_order.ClearData();
			_runner->notify_touch_cost(income,remain,cost,0,0,retcode);
			break;
	}
}

void 
gplayer_imp::OnAuAddupMoneyQuery(int64_t addupmoney)
{
	_runner->notify_addup_money(addupmoney);
}

void 
gplayer_imp::PlayerTouchPointQuery()
{
	if(!_touch_order.Query(this))
	{
		_runner->notify_touch_query(_touch_order.income,_touch_order.remain,1); // busy
	}
}

void 
gplayer_imp::PlayerTouchPointCost(unsigned int index,int type,unsigned int count,unsigned int price,int expire,unsigned int lots)
{
	if(type <= 0 || count <= 0 || lots <= 0) return;

	float total_costf = (float)lots*(float)price;
	if(total_costf > 2e9) // unsigned int max
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,lots*price,index,lots,touch_trade::TPC_NEED_MORE);
		return;
	}

	unsigned int total_cost32 = lots*price;

	if(_touch_order.remain < total_cost32)
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_NEED_MORE);
		return;
	}

	if(!_touch_order.IsFree())
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_BUSY);
		return;
	}

	if(!world_manager::GetTouchShop().CheckGoods(index,type,count,price,expire))
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_OTHER);
		return;
	}

	if(OI_TestSafeLock() || _lock_inventory)
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_OTHER);
		return;
	}

	int pile_limit = world_manager::GetDataMan().get_item_pile_limit(type);
	if( 0 >= pile_limit)
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_OTHER);
		return;
	}

	unsigned int needslots = ((count*lots + pile_limit - 1)/pile_limit);	
	if(!InventoryHasSlot(needslots))
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_NEED_SLOT);
		return;
	}

	if(!_touch_order.TryCost(this,index,type,count,total_cost32,expire,lots))
	{
		_runner->notify_touch_cost(_touch_order.income,_touch_order.remain,total_cost32,index,lots,touch_trade::TPC_BUSY);
	}
}

void 
gplayer_imp::PlayerRedeemGiftCard(const char(&cn)[player_giftcard::GIFT_CARDNUMBER_LEN])
{
	int retcode = 0;
	if((retcode = _player_giftcard.TryRedeem(this,cn)) != 0)
	{
		_runner->notify_giftcard_redeem(retcode,0,0,cn);	
	}
}

void 
gplayer_imp::OnGiftCardRedeem(const char(&cn)[player_giftcard::GIFT_CARDNUMBER_LEN],int type,int parenttype,int retcode)
{
	_player_giftcard.OnRedeem(this,cn,type,parenttype,retcode);
}

void gplayer_imp::PlayerQueryTitle(int roleid)
{
	_player_title.QueryTitleData(_runner);
}

void gplayer_imp::PlayerChangeTitle(TITLE_ID titleid)
{
	_player_title.ChangeCurrTitle(titleid);
}

void gplayer_imp::UpdateDisplayTitle(TITLE_ID titleid)
{
	GetParent()->title_id = titleid;

	if(titleid)
		GetParent()->object_state2 |= gactive_object::STATE_TITLE;
	else
		GetParent()->object_state2 &= ~gactive_object::STATE_TITLE;
}

void gplayer_imp::OnObtainRareTitle(TITLE_ID titleid)
{
	struct 
	{
		unsigned short titleid;
		char name[MAX_USERNAME_LENGTH];
	}data;
	memset(&data,0,sizeof(data));

	data.titleid = titleid;
	size_t  len = _username_len;
	if(len > MAX_USERNAME_LENGTH) len = MAX_USERNAME_LENGTH;
	memcpy(data.name, _username,len);

	broadcast_chat_msg(TITLE_RARE_CHAT_MSG_ID, &data, sizeof(data), GMSV::CHAT_CHANNEL_SYSTEM, 0, 0, 0);
	__PRINTF("�����ƶ��㲥\n");
}

void gplayer_imp::PlayerDailySignin()
{
	if(_player_dailysign.DaySignIn(player_dailysign::MK_LOCAL_TIME()))
	{
		 GLog::log(LOG_INFO,"roleid:%d daily signin success",_parent->ID.id);	
	}
}

void gplayer_imp::PlayerLateSignin(char type,int itempos,int desttime)
{
	if(_lock_inventory || OI_TestSafeLock())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return;
	}
	
	item_list& inv = GetInventory();

	if(0 == type) // Ŀǰֻʵ�ֵ����ղ�ǩ
	{
		if(itempos == -1)  itempos = inv.Find(0, LATE_DAY_SIGNIN_ITEM);   // �ƶ��ͻ�����Ҫϵͳ��λ
		
		if( inv.IsItemExist(itempos, LATE_DAY_SIGNIN_ITEM ,1) &&
				_player_dailysign.DaySignIn(desttime))
		{
			item& it = inv[itempos];
			UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
				
			_runner->player_drop_item(gplayer_imp::IL_INVENTORY, itempos, it.type, 1, S2C::DROP_TYPE_TAKEOUT);
			inv.DecAmount(itempos,1);
			
			GLog::log(LOG_INFO,"roleid:%d late signin success",_parent->ID.id);
		}
	}
	else if(1 == type && player_template::GetDebugMode()) // ������
	{
		_player_dailysign.MonthSignIn(desttime);
		GLog::log(LOG_INFO,"roleid:%d month late signin success",_parent->ID.id); 
	}
	else if(2 == type && player_template::GetDebugMode()) // ������
	{
		_player_dailysign.YearSignIn(desttime);
		GLog::log(LOG_INFO,"roleid:%d year late signin success",_parent->ID.id); 
	}
}

void gplayer_imp::PlayerApplySigninAward(char type ,int mon)
{
	if(_lock_inventory || OI_TestSafeLock())
	{
		_runner->error_message(S2C::ERR_INVENTORY_IS_LOCKED);
		return;
	}

	_player_dailysign.RequestAwards(type, mon);
}

void gplayer_imp::PlayerRefreshSignin()
{
	_player_dailysign.CheckPoint();
	_player_dailysign.ClientSync(player_dailysign::SYNC4INIT);
}

void gplayer_imp::PlayerSwitchInParallelWorld(const instance_hash_key & key)
{
	if((_player_state != PLAYER_STATE_NORMAL
				&& _player_state != PLAYER_SIT_DOWN) 
				|| IsCombatState())
	{
		_runner->error_message(S2C::ERR_CANNOT_SWITCH_IN_PARALLEL_WORLD);
		return;
	}
	if(!CheckCoolDown(COOLDOWN_INDEX_SWITCH_IN_PARALLEL_WORLD))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return;
	}
	LeaveAbnormalState();
	ASSERT(_player_state == PLAYER_STATE_NORMAL);
	SetCoolDown(COOLDOWN_INDEX_SWITCH_IN_PARALLEL_WORLD, SWITCH_IN_PARALLEL_WORLD_COOLDOWN_TIME);

	int rst = world_manager::GetInstance()->PlayerSwitchWorld(GetParent(), key);
	if(rst > 0) _runner->error_message(rst);
}

void gplayer_imp::PlayerQueryParallelWorld()
{
	if(!CheckCoolDown(COOLDOWN_INDEX_QUERY_PARALLEL_WORLD))
	{
		_runner->error_message(S2C::ERR_OBJECT_IS_COOLING);
		return;
	}
	SetCoolDown(COOLDOWN_INDEX_QUERY_PARALLEL_WORLD, QUERY_PARALLEL_WORLD_COOLDOWN_TIME);
	
	world_manager::GetInstance()->PlayerQueryWorld(GetParent());
}

void gplayer_imp::PlayerReincarnation()
{
	//����״̬�½�ֹת��,��ֹת������Ϊmp�����޷�ʹ�ü��ָܻ�����״̬
	if(GetForm()) return;
	
	if(!_player_reincarnation.CheckCondition())
	{
		_runner->error_message(S2C::ERR_REINCARNATION_CONDITION);
		return;
	}

	//���ͨ������ʼת��
	_player_reincarnation.DoReincarnation();

	RefreshEquipment();
	CalcEquipmentInfo();
	PlayerGetProperty();

	_petman.RecallPet(this);

	if(!world_manager::GetWorldParam().pve_mode)
	{
		TestPKProtected();
	}
	GMSV::SendSynMutaData(_parent->ID.id,_basic.level, _player_reincarnation.GetTimes());
	_level_up  = true;
}

void gplayer_imp::PlayerGetReincarnationTome()
{
	_player_reincarnation.ClientGetTome();
}

void gplayer_imp::PlayerRewriteReincarnationTome(size_t record_index, int record_level)
{
	if(!_player_reincarnation.RewriteTome(record_index, record_level))
	{
		_runner->error_message(S2C::ERR_REINCARNATION_REWRITE_TOME);
		return;
	}

	//��дת���������ʷ��ߵȼ���������,�������������ȼ�/�����ȼ�
	gplayer * pPlayer = GetParent();
	int prev_invisible_degree = pPlayer->invisible_degree;
	int prev_anti_invisible_degree = pPlayer->anti_invisible_degree;
	bool invisible_changed = false;
	property_policy::UpdatePlayerInvisible(this);
	if(pPlayer->IsInvisible() && pPlayer->invisible_degree > prev_invisible_degree)
	{
		_runner->on_inc_invisible(prev_invisible_degree,pPlayer->invisible_degree);
		__PRINTF("����%d��������\n",pPlayer->invisible_degree);
		invisible_changed = true;
	}
	if(pPlayer->anti_invisible_degree > prev_anti_invisible_degree)
	{
		__PRINTF("�����ȼ�%d\n",pPlayer->anti_invisible_degree);
		_runner->on_inc_anti_invisible(prev_anti_invisible_degree,pPlayer->anti_invisible_degree);
		invisible_changed = true;
	}
	if(invisible_changed) _petman.NotifyInvisibleData(this);
}

void gplayer_imp::PlayerActiveReincarnationTome(bool b)
{
	if(!b)
	{
		//ȡ������
		_player_reincarnation.DeactivateTome();
		return;
	}
	//����	
	if(!_player_reincarnation.TryActivateTome())
	{
		_runner->error_message(S2C::ERR_REINCARNATION_ACTIVE_TOME);
	}
}

void gplayer_imp::OnAutoTeamPlayerReady(int leader_id)
{
	int retcode = -1;
	int world_tag = world_manager::GetWorldTag();
	//��ҽ������ڴ��ͼ �����ع� ��ħ�ع� ת���ع� ���͡�� �����Զ����
	if(world_tag == 1 || world_tag == 121 || world_tag == 122 
		|| world_tag == 137 || world_tag == 161 || world_tag == 163) 
	{
		retcode = _team.OnAutoTeamPlayerReady(leader_id);
	}

	GMSV::SendAutoTeamPlayerReady_Re(_parent->ID.id, leader_id, retcode);
}

void gplayer_imp::OnAutoTeamComposeFailed(int leader_id)
{
	_team.OnAutoTeamComposeFailed(leader_id);
}

void gplayer_imp::OnAutoTeamComposeStart(int member_list[], unsigned int cnt)
{
	_team.OnAutoTeamComposeStart(member_list, cnt);
}

void gplayer_imp::PlayerSetAutoTeamGoal(char goal_type, char op, int goal_id)
{
	if(_team.IsInTeam()) return;
	
	int world_tag = world_manager::GetWorldTag();
	//û�ж������ҿ����ڴ��ͼ�������ع�����ħ�ع���ת���µع� ���͡�� ������������
	if(op > 0 && world_tag != 1 && world_tag != 121 && world_tag != 122 && world_tag != 137 && world_tag != 161 && world_tag != 163) return;
	if(goal_id <= 0) return;
	if(goal_type <= player_team::auto_team_info_t::GOAL_TYPE_INVALID || goal_type > player_team::auto_team_info_t::GOAL_TYPE_ACTIVITY) return;

	if((goal_type == player_team::auto_team_info_t::GOAL_TYPE_ACTIVITY) && (op > 0))
	{
		if(!CheckCoolDown(COOLDOWM_INDEX_AUTOTEAM_SET_GOAL)) return;
		bool ret = world_manager::GetAutoTeamMan().CanPlayerAutoComposeTeam(goal_id, this);
		if(!ret) return;
		SetCoolDown(COOLDOWM_INDEX_AUTOTEAM_SET_GOAL, AUTOTEAM_SET_GOAL_COOLDOWN_TIME);
	}

	GMSV::SendPlayerSetAutoTeamGoal(_parent->ID.id, goal_type, op, goal_id);
}

void gplayer_imp::PlayerJumpToGoal(int goal_id)
{
	if(_player_state != PLAYER_STATE_NORMAL 
			&& _player_state != PLAYER_STATE_BIND
			&& _player_state != PLAYER_SIT_DOWN
			&& _player_state != PLAYER_STATE_MARKET)
	{
		return;
	}
	if(IsCombatState() || HasSession()) return;
	if(!CheckCoolDown(COOLDOWM_INDEX_AUTOTEAM_SET_GOAL)) return;
	
	int current_worldtag = world_manager::GetWorldTag();
	int target_tag = -1;
	A3DVECTOR target_pos;
	bool ret = world_manager::GetAutoTeamMan().GetGoalEntrancePos(goal_id, this, current_worldtag, target_tag, target_pos);
	if(!ret) return;

	LeaveAbnormalState();
	
	int jump_item_idx = -1;
	for(unsigned int i = 0; i < _inventory.Size(); ++i)
	{
		item& it = _inventory[i];
		if(it.type == -1) continue;
		if((it.type == AUTO_TEAM_JUMP_ITEM1) || (it.type == AUTO_TEAM_JUMP_ITEM2) 
				|| (it.type == AUTO_TEAM_JUMP_ITEM3) || (it.type == AUTO_TEAM_JUMP_ITEM4))
		{
			if((jump_item_idx == -1) || (it.type < _inventory[jump_item_idx].type))
			{
				jump_item_idx = i;
				if(it.type == AUTO_TEAM_JUMP_ITEM1) break;
			}
		}
	}
	
	if(jump_item_idx >= 0)
	{
		item& it = _inventory[jump_item_idx];
		int item_id = it.type;
		UpdateMallConsumptionDestroying(it.type, it.proc_type, 1);
		
		_inventory.DecAmount(jump_item_idx, 1);
		_runner->player_drop_item(IL_INVENTORY, jump_item_idx, item_id, 1, S2C::DROP_TYPE_TAKEOUT);

		SetCoolDown(COOLDOWM_INDEX_AUTOTEAM_SET_GOAL, AUTOTEAM_SET_GOAL_COOLDOWN_TIME);
		LongJump(target_pos, target_tag);
	}
}


int gplayer_imp::PlayerTrickBattleApply(int chariot)
{
	if(GetHistoricalMaxLevel() < TRICKBATTLE_LEVEL_REQUIRED) return S2C::ERR_LEVEL_NOT_MATCH;
	if(_basic.sec_level < TRICKBATTLE_SEC_LEVEL_REQUIRED) return S2C::ERR_SEC_LEVEL_NOT_MATCH;
	if(!CheckCoolDown(COOLDOWM_INDEX_TRICKBATTLE_APPLY)) return S2C::ERR_OBJECT_IS_COOLING;

	DATA_TYPE dt;
	CHARIOT_CONFIG * cfg = (CHARIOT_CONFIG *)world_manager::GetDataMan().get_data_ptr(chariot, ID_SPACE_CONFIG, dt);
	if(!cfg || dt != DT_CHARIOT_CONFIG) return -1;
	if(cfg->pre_chariot > 0) return -1;	//ֻ��ѡ�����ս��

	GMSV::SendTrickBattleApply(_parent->ID.id, chariot);
	SetCoolDown(COOLDOWM_INDEX_TRICKBATTLE_APPLY, TRICKBATTLE_APPLY_COOLDOWN_TIME);
	return 0;
}

void gplayer_imp::PlayerLeaveTrickBattle()
{
	if(world_manager::GetInstance()->GetWorldType() != WORLD_TYPE_TRICKBATTLE) return;
	
	_filters.ModifyFilter(FILTER_CHECK_INSTANCE_KEY,FMID_CLEAR_AETB,NULL,0);	
}

void gplayer_imp::EnterTrickBattleStep1(int target_tag, int battle_id, int chariot)
{
	if(_player_state != PLAYER_STATE_NORMAL)
	{
		return;
	}

	if(target_tag == world_manager::GetWorldTag()) return ;
	
	instance_key key;
	memset(&key,0,sizeof(key));
	GetInstanceKey(target_tag, key);
	key.target = key.essence;

	key.target.key_level4 = battle_id;

	ClearSwitchAdditionalData();
	A3DVECTOR pos(0,0,0);
	//��Player���и������� 
	if(world_manager::GetInstance()->PlaneSwitch(this ,pos,target_tag,key,0) < 0)
	{
		return ;
	}
	_switch_additional_data = new trickbattle_switch_data(chariot);
}

void gplayer_imp::EnterTrickBattleStep2()
{
	trickbattle_switch_data * pData = substance::DynamicCast<trickbattle_switch_data>(_switch_additional_data);
	ASSERT(pData);

	TrickBattleTransformChariot(pData->chariot);

	ClearSwitchAdditionalData();
}

void gplayer_imp::ReceiveRealmExp(int exp)
{
	_realm_exp += exp;
	
	bool levelup =false;
	do
	{
		int levelup_exp = player_template::GetRealmLvlupExp(_realm_level);
		if(_realm_exp < levelup_exp) break;
		if(_realm_level %10 == 0)
		{
			exp -= (_realm_exp - levelup_exp);
			_realm_exp = levelup_exp;			
			break;
		}

		_realm_exp -= levelup_exp;
		_realm_level ++;
		levelup = true;
	}while(1);
	
	if(levelup)
	{
		UpdateRealmLevel();
	}

 	_runner->realm_exp_receive(_realm_exp, exp);
}

void gplayer_imp::UpdateRealmLevel()
{
	GetParent()->realmlevel = _realm_level;
	GetParent()->object_state2 |= gactive_object::STATE_REALMLEVEL;
	
	_runner->realm_level_up(_realm_level);

	SetVigourBase( player_template::GetRealmVigour(_realm_level) );
	
	PlayerGetProperty();
}

bool gplayer_imp::ExpandRealmLevelMax()
{
	if(!IsRealmExpFull())
		return false;
	
	_realm_exp = 0;
	_runner->realm_exp_receive(_realm_exp, 0);
	++_realm_level; 
	UpdateRealmLevel();

	return true;
}

bool gplayer_imp::IsRealmExpFull()
{
	return _realm_level%10 == 0 
		&& _realm_level < player_template::GetMaxRealmLevelLimit() 
		&& _realm_exp >= player_template::GetRealmLvlupExp(_realm_level);
}

int gplayer_imp::GetObtainedGeneralCardCountByRank(int rank)
{
	if (rank < 0 || rank > 4)
		return 0;

	int count = 0;
	size_t size = _inventory.Size();
	for(size_t i = 0; i < size; ++i)
	{
		item& it = _inventory[i];

		if (it.type <= 0 || it.body == NULL || 
				it.body->GetItemType() != item_body::ITEM_TYPE_GENERALCARD)
			continue;
		
		if (it.body->GetRank() == rank)
			count ++;
	}

	for(size_t i = item::EQUIP_INDEX_GENERALCARD1; i < item::EQUIP_INDEX_GENERALCARD6 + 1; i++)
	{
		item& it = _equipment[i];
		
		if (it.type <= 0 || it.body == NULL || 
				it.body->GetItemType() != item_body::ITEM_TYPE_GENERALCARD)
			continue;
		
		if (it.body->GetRank() == rank)
			count ++;
	}
	
	item_list & backpack4 = _trashbox.GetBackpack4();
	size = backpack4.Size();
	for(size_t i = 0; i < size; ++i)
	{
		item& it = backpack4[i];
		
		if (it.type <= 0 || it.body == NULL || 
				it.body->GetItemType() != item_body::ITEM_TYPE_GENERALCARD)
			continue;
		
		if (it.body->GetRank() == rank)
			count ++;
	}

	return count;
}

int gplayer_imp::PlayerGeneralCardRebirth(size_t major_inv_idx, size_t minor_inv_idx)
{
	if(major_inv_idx >= _inventory.Size() || minor_inv_idx >= _inventory.Size()) 
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	item & it_major = _inventory[major_inv_idx];
	item & it_minor = _inventory[minor_inv_idx];
	if(it_major.type != it_minor.type
			|| it_major.type <= 0 || it_major.body == NULL || it_major.body->GetItemType() != item_body::ITEM_TYPE_GENERALCARD
			|| it_minor.type <= 0 || it_minor.body == NULL || it_minor.body->GetItemType() != item_body::ITEM_TYPE_GENERALCARD)
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	if(!it_major.CheckRebirthCondition(it_minor.GetRebirthTimes()))
		return S2C::ERR_GENERALCARD_REBIRTH_CONDITION;

	if(it_major.GetRank() >= GENERALCARD_RANK_S)
	{
		//S�����Ͽ���ת����log
		GLog::formatlog("rebirthcard:roleid=%d:cardid=%d", _parent->ID.id, it_major.type);
	}

	UpdateMallConsumptionDestroying(it_minor.type, it_minor.proc_type, 1);
	_runner->player_drop_item(IL_INVENTORY, minor_inv_idx, it_minor.type, 1, S2C::DROP_TYPE_TAKEOUT);
	_inventory.DecAmount(minor_inv_idx, 1);

	it_major.DoRebirth();
	PlayerGetItemInfo(IL_INVENTORY, major_inv_idx);
	return 0;
}

int gplayer_imp::PlayerSwallowGeneralCard(size_t equip_idx, bool is_inv, size_t inv_idx, size_t count)
{
	item_list * pInv = NULL;
	if(is_inv) 
		pInv = &GetInventory();
	else
		pInv = &GetTrashInventory(IL_TRASH_BOX4);

	if(equip_idx < item::EQUIP_INDEX_GENERALCARD1 
			|| equip_idx > item::EQUIP_INDEX_GENERALCARD6
			|| inv_idx >= (*pInv).Size()) 
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	item & it_eq = _equipment[equip_idx];
	item & it_inv = (*pInv)[inv_idx];
	if(it_eq.type <= 0 || it_inv.type <= 0 || !count || it_inv.count < count) 
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	
	DATA_TYPE dt;
	const void * ess = world_manager::GetDataMan().get_data_ptr(it_inv.type, ID_SPACE_ESSENCE, dt);
	if(!ess) return S2C::ERR_ITEM_NOT_IN_INVENTORY;

	int exp = 0;
	if(dt == DT_POKER_ESSENCE) 
		exp = ((const POKER_ESSENCE *)ess)->swallow_exp;
	else if(dt == DT_POKER_DICE_ESSENCE) 
		exp = ((const POKER_DICE_ESSENCE*)ess)->swallow_exp;
	else 
		return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	//���ɿ���ʱҪ����ÿ�����ǰ������ת�����ĵľ���
	if(dt == DT_POKER_ESSENCE) 
		exp += it_inv.GetSwallowExp();

	if(exp <= 0) return S2C::ERR_GENERALCARD_INSERT_EXP;
	float tmp = (float)exp * count;
	if(tmp > 2e9) return S2C::ERR_GENERALCARD_INSERT_EXP;
	exp *= count;
	
	//ֻ������Ʒ��С�ڵ��������Ŀ���	
	if(dt == DT_POKER_ESSENCE && it_inv.GetRank() > it_eq.GetRank()) 
		return S2C::ERR_GENERALCARD_INSERT_EXP;
	
	//����Ƿ�������Ӿ���
	if(!it_eq.InsertExp(exp, true))
	{
		return S2C::ERR_GENERALCARD_INSERT_EXP;
	}

	if(dt == DT_POKER_ESSENCE && it_inv.GetRank() >= GENERALCARD_RANK_S)
	{
		//����S�����Ͽ���LOG
		//GLog::formatlog("swallowcard:roleid=%d:cardid=%d:exp=%d", _parent->ID.id, it_inv.type, exp);

        // ��������S�����Ͽ���
        return S2C::ERR_GENERALCARD_INSERT_EXP;
	}

	it_eq.Deactivate(item::BODY, equip_idx, this);
	it_eq.InsertExp(exp, false);	
	it_eq.Activate(item::BODY, _equipment, equip_idx, this);

	PlayerGetItemInfo(IL_EQUIPMENT, equip_idx);
	RefreshEquipment();

	UpdateMallConsumptionDestroying(it_inv.type, it_inv.proc_type, count);
	_runner->player_drop_item((is_inv ? IL_INVENTORY : IL_TRASH_BOX4), inv_idx, it_inv.type, count, S2C::DROP_TYPE_TAKEOUT);
	(*pInv).DecAmount(inv_idx, count);
	//�����ֿ���޸Ĳ���Ҫ�򿪲ֿ⣬��Ҫ���Ӹı����
	if(!is_inv) IncTrashBoxChangeCounter();
	return 0;
}

void gplayer_imp::PlayerQueryChariots()
{
	//�ɿͻ����������𣬿���Ҫ�Ӽ�ⲻ��������ȴ
	QueryTrickBattleChariots();
}

int gplayer_imp::PlayerImproveFlysword(size_t inv_idx, int flysword_id)
{
	//�����Ʒ�Ƿ����
	if(inv_idx >= _inventory.Size()) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	item & it = _inventory[inv_idx];
	if(it.type == -1 || it.type != flysword_id) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	//���ɽ��Ƿ�ɸ���
	DATA_TYPE dt;
	FLYSWORD_ESSENCE * ess = (FLYSWORD_ESSENCE *)world_manager::GetDataMan().get_data_ptr(flysword_id, ID_SPACE_ESSENCE, dt);
	if(ess == NULL || dt != DT_FLYSWORD_ESSENCE) return S2C::ERR_ITEM_NOT_IN_INVENTORY;
	if(ess->max_improve_level <= 0 || (size_t)ess->max_improve_level > sizeof(ess->improve_config)/sizeof(ess->improve_config[0])) return S2C::ERR_ITEM_CANNOT_IMPROVE;

	int ilevel = it.GetImproveLevel();
	if(ilevel >= ess->max_improve_level) return S2C::ERR_ITEM_CANNOT_IMPROVE;
	//�������Ƿ��㹻
	size_t icost = ess->improve_config[ilevel].require_item_num;
	int ticket_list[] = {IMPROVE_FLYSWORD_TICKET_ID1, IMPROVE_FLYSWORD_TICKET_ID2, IMPROVE_FLYSWORD_TICKET_ID3};
	if(icost == 0 || !CheckItemExist(ticket_list, sizeof(ticket_list)/sizeof(int), icost)) return S2C::ERR_NOT_ENOUGH_MATERIAL; 
	//���и���
	if(!it.FlyswordImprove(ess->improve_config[ilevel].speed_increase, ess->improve_config[ilevel].speed_rush_increase)) return S2C::ERR_ITEM_CANNOT_IMPROVE;
	//���Ĳ���
	TakeOutItem(ticket_list, sizeof(ticket_list)/sizeof(int), icost);
	//�ͻ��˸��·ɽ�
	PlayerGetItemInfo(IL_INVENTORY, inv_idx);
	return 0;
}

void gplayer_imp::PlayerRandMallQuery(int eid)
{
	DATA_TYPE dt;
	RAND_SHOP_CONFIG * ess = (RAND_SHOP_CONFIG*) world_manager::GetDataMan().get_data_ptr(eid, ID_SPACE_CONFIG, dt);

	if(ess == NULL || dt != DT_RAND_SHOP_CONFIG)
	{
		_runner->random_mall_shopping_result(eid,random_mall_info::RAND_MALL_OPT_QUERY,
				random_mall_info::RM_ENTRY_ERR,0,0,false);
		return ;
	}

	random_mall_info::random_mall_result record;
	bool firstflag = !_player_randmall.QueryResult(eid,record);	
	_runner->random_mall_shopping_result(eid,random_mall_info::RAND_MALL_OPT_QUERY,
			0, record.itemid, record.price, firstflag);
}

int RandSelectInRange(const void * prob, int stride, size_t beg, size_t end)
{	
	int count = end - beg;
	if(count == 1) return beg;
	ASSERT(prob && stride >= (int)sizeof(float) && count > 0);
	float* prolist = (float*)abase::fast_allocator::align_alloc(count*sizeof(float));

	const char * option = (const char*)prob;
	float total_pro = 0;
	for(size_t i = beg; i < end; i++,option += stride)
	{
		float tmp = *(float*)option; ASSERT(tmp >= 0 && tmp <=1.f);
		prolist[i-beg] = tmp;
		total_pro += tmp; ASSERT(total_pro <=1.f);
	}

	float total_pro_div = 1.f/total_pro;
	total_pro = 0;
	for(int n = 0; n < count - 1; n++)
	{
		prolist[n] *= total_pro_div;
		total_pro += prolist[n];
	}
	prolist[count - 1] = 1.f - total_pro;

	int idx = abase::RandSelect(prolist, sizeof(float), count); 
	abase::fast_allocator::align_free(prolist,count*sizeof(float));

	return beg + idx;
}

void gplayer_imp::PlayerRandMallRoll(int eid)
{
#define RM_ROLL_ERR_RET(re) { _runner->random_mall_shopping_result(eid,random_mall_info::RAND_MALL_OPT_ROLL,re,0,0,false);return; }
	if(_lock_inventory || OI_TestSafeLock())
	{
		_runner->error_message(S2C::ERR_FORBIDDED_OPERATION_IN_SAFE_LOCK);
		return;
	}
	
	DATA_TYPE dt;
	const RAND_SHOP_CONFIG * ess = (RAND_SHOP_CONFIG*) world_manager::GetDataMan().get_data_ptr(eid, ID_SPACE_CONFIG, dt);

	if(ess == NULL || dt != DT_RAND_SHOP_CONFIG || ess->first_buy_price < 0 || ess->price <= 0)
		RM_ROLL_ERR_RET(random_mall_info::RM_ENTRY_ERR)

	if(InCentralServer() || GetHistoricalMaxLevel() < random_mall_info::RAND_MALL_LEVEL_LIMIT)
		RM_ROLL_ERR_RET(random_mall_info::RM_ROLE_STATE_ERR)
	
	random_mall_info::random_mall_result record;
	bool firstflag = !_player_randmall.QueryResult(eid,record);	
	
	if(!firstflag && record.itemid)
		RM_ROLL_ERR_RET(random_mall_info::RM_OPT_STATE_ERR)

	// ��ʼroll 
	if(firstflag)
	{
		size_t beg = 0;
		size_t end = beg + ess->first_buy_range;
		beg = std::min(beg, sizeof(ess->list)/sizeof(ess->list[0]) - 1);
		end = std::min(end, sizeof(ess->list)/sizeof(ess->list[0]));
		ASSERT(beg < end);
		int idx = RandSelectInRange(&(ess->list[0].probability), sizeof(ess->list[0]), beg, end);		
		record.itemid = ess->list[idx].id;
		record.price = ess->first_buy_price;
	}
	else
	{
		int idx = abase::RandSelect(&(ess->list[0].probability), sizeof(ess->list[0]),sizeof(ess->list)/sizeof(ess->list[0]));
		record.itemid = ess->list[idx].id;
		record.price = ess->price;
	}

	_player_randmall.PendingPay(eid, record);
	_runner->random_mall_shopping_result(eid,random_mall_info::RAND_MALL_OPT_ROLL, 0, record.itemid, record.price, firstflag);
	
	GLog::log(GLOG_INFO,"�û�%d������̳�%d Roll����Ʒ%d���۸�%d��",GetParent()->ID.id, eid, record.itemid, record.price);
#undef RM_ROLL_ERR_RET
}

void gplayer_imp::PlayerRandMallPay(int eid)
{
#define RM_PAY_ERR_RET(re) { _runner->random_mall_shopping_result(eid,random_mall_info::RAND_MALL_OPT_PAY,re,0,0,false);return; }
	
	if(_lock_inventory || OI_TestSafeLock())
	{
		_runner->error_message(S2C::ERR_FORBIDDED_OPERATION_IN_SAFE_LOCK);
		return;
	}

	if(InCentralServer() || GetHistoricalMaxLevel() < random_mall_info::RAND_MALL_LEVEL_LIMIT)
		RM_PAY_ERR_RET(random_mall_info::RM_ROLE_STATE_ERR)

	random_mall_info::random_mall_result record;
	_player_randmall.QueryResult(eid,record);	

	if(0 == record.itemid)
		RM_PAY_ERR_RET(random_mall_info::RM_OPT_STATE_ERR)

	if(record.price < 0 || record.price > GetMallCash())
		RM_PAY_ERR_RET(random_mall_info::RM_CASH_ERR)	

	if(!InventoryHasSlot(1))
		RM_PAY_ERR_RET(random_mall_info::RM_INV_ERR)
	
	element_data::item_tag_t tag = {element_data::IMT_SHOP, 0};
	item_data * pItem = world_manager::GetDataMan().generate_item_from_player(record.itemid, &tag, sizeof(tag));
	if(pItem)
	{
		int total_cash = GetMallCash();
		int cash_used = 0;
		//��Ǯ�㹻�� ��ʼ������Ʒ
		int self_id = GetParent()->ID.id;
		int count = 1;
		int expire_date = 0;
		int log_price1 = record.price;
		int id = record.itemid;

		pItem->proc_type |= item::ITEM_PROC_TYPE_MALL;
		UpdateMallConsumptionShopping(pItem->type, pItem->proc_type, count, log_price1);
		
		int ocount = count;
		int rst =_inventory.Push(*pItem,count,expire_date);
		ASSERT(rst >= 0 && count == 0);
		_runner->obtain_item(id,pItem->expire_date,ocount,_inventory[rst].count, 0,rst);

		//�������³�ʼ��һ�¿��ܵ�ʱװ
		_inventory[rst].InitFromShop();

		FirstAcquireItem(pItem);					
		
		total_cash -= log_price1;
		cash_used += log_price1;

		//��¼��־  
		GLog::formatlog("formatlog:gshop_trade:userid=%d:db_magic_number=%d:order_id=%d:item_id=%d:expire=%d:item_count=%d:cash_need=%d:cash_left=%d:guid1=%d:guid2=%d",
				self_id,_db_user_id,_mall_order_id,id,expire_date,ocount,log_price1,total_cash,pItem->guid.guid1,pItem->guid.guid2);
		
		world_manager::TestCashItemGenerated(id, ocount);
		FreeItem(pItem);

		_mall_cash_offset -= cash_used;
		_runner->player_cash(GetMallCash());
		//�����Ѽ�¼���͸�gdelivery,�������ӿ��Ը������ߵĺ������������ѻ���
		GMSV::SendRefCashUsed(_parent->ID.id, cash_used, _basic.level);

		GLog::log(GLOG_INFO,"�û�%d������̳�%d����%d����Ʒ%d������%d��ʣ��%d��",self_id,eid,ocount,id,cash_used,GetMallCash());

		_mall_order_id ++;
	}
	else
	{
		GLog::log(GLOG_ERR,"�û�%d������̳�%d������Ʒ%dʧ��",GetParent()->ID.id,eid,record.itemid);
	}

	_player_randmall.ClosingPay(eid);	
	_runner->random_mall_shopping_result(eid,random_mall_info::RAND_MALL_OPT_PAY, 0, record.itemid, record.price, false);

#undef RM_PAY_ERR_RET
}

namespace
{
	struct mafia_staff_collector
	{
		int _self_id;
		int _mafia_id;
		float _squared_radius;
		std::vector<GMSV::MafiaMemberInfo> &_list;
		mafia_staff_collector(int self,int mid, float radius, std::vector<GMSV::MafiaMemberInfo> &list)
			: _self_id(self),_mafia_id(mid),_squared_radius(radius*radius),_list(list){}

		inline void operator()(slice *pPiece,const A3DVECTOR & pos)
		{
			if(!pPiece->player_list) return;
			pPiece->Lock();
			gplayer * pPlayer = (gplayer *)pPiece->player_list;
			while(pPlayer)
			{
				if(pPlayer->ID.id != _self_id && pPlayer->base_info.level >= MAFIA_PVP_LEVEL_LIMIT 
				&& pPlayer->id_mafia == _mafia_id && pos.squared_distance(pPlayer->pos) < _squared_radius)
				{
					GMSV::MafiaMemberInfo member;
					member.roleid = pPlayer->ID.id;
					member.rank = pPlayer->rank_mafia;
					_list.push_back(member);
				}
				pPlayer = (gplayer *)pPlayer->pNext;
			}
			pPiece->Unlock();
		}
	};
}

void gplayer_imp::OnMafiaPvPAward(int type,const XID& sourceid, const A3DVECTOR& pos, int mafiaid, int domainid)
{
	int selfmafia = OI_GetMafiaID();

	if(!world_manager::GetWorldFlag().mafia_pvp_flag || !selfmafia || _basic.level < MAFIA_PVP_LEVEL_LIMIT || 0 == ((gplayer*)_parent)->mafia_pvp_mask)
	{
		GLog::log(GLOG_ERR,"�û�%d�ڰ���pvpδ�в����ʸ�ʱ�ܵ�����%d [ms:%d md:%d d:%d l:%d]",_parent->ID.id,type,selfmafia,mafiaid,domainid,_basic.level);

		if(type == AWARD_MAFIAPVP_MINEBASE) // �����¼�������Ҫͬ��map ��ʾ���¼�����֪ͨ
			GMSV::SendMafiaPvPEvent(AWARD_MAFIAPVP_NO_OWNER_MINEBASE,0,mafiaid,0,0,domainid);
		else if(type == AWARD_MAFIAPVP_MINECAR) // ���¼�������Ҫ������ص÷ֹ��¼�����֪ͨ
			GMSV::SendMafiaPvPEvent(AWARD_MAFIAPVP_NO_OWNER_MINECAR,0,mafiaid,0,0,domainid);
		return;
	}

	switch(type)
	{
		case AWARD_MAFIAPVP_MINECAR:
		case AWARD_MAFIAPVP_MINEBASE:
			{
				std::vector<GMSV::MafiaMemberInfo> list;
				mafia_staff_collector worker(_parent->ID.id,selfmafia,NORMAL_EXP_DISTANCE,list);
				_plane->ForEachSlice(pos,NORMAL_EXP_DISTANCE,worker);

				GMSV::SendMafiaPvPEvent(type,selfmafia,mafiaid,_parent->ID.id,((gplayer*)_parent)->rank_mafia,domainid,&list);	
			}
			break;
		case AWARD_MAFIAPVP_HIJACK_KILL:
			{
				std::vector<GMSV::MafiaMemberInfo> list;
				GMSV::MafiaMemberInfo victim;
				victim.roleid = sourceid.id; 
				victim.rank = 0;
				list.push_back(victim);
				GMSV::SendMafiaPvPEvent(type,selfmafia,mafiaid,_parent->ID.id,((gplayer*)_parent)->rank_mafia,domainid,&list);	
			}
			break;
		case AWARD_MAFIAPVP_MINECAR_PROTECT:
			{
				GMSV::SendMafiaPvPEvent(type,selfmafia,mafiaid,_parent->ID.id,((gplayer*)_parent)->rank_mafia,domainid);		
			}
			break;
	}
}

void gplayer_imp::PlayerQueryMafiaPvPInfo()
{
	if(!world_manager::GetWorldFlag().mafia_pvp_flag) return;
	
	int selfmafia = OI_GetMafiaID();
	if(selfmafia && CheckCoolDown(COOLDOWN_INDEX_QUERY_MAFIA_PVP_INFO))
	{
		GMSV::SendMafiaPvPQuery(_parent->ID.id, selfmafia);
		SetCoolDown(COOLDOWN_INDEX_QUERY_MAFIA_PVP_INFO,PLAYER_QUERY_MAFIA_PVP_COOLDOWN_TIME);
	}
}

void gplayer_imp::copy_other_role_data(int src_roleid)
{
    class CopyCallBack : public GDB::CopyRoleResult, public abase::ASmallObject
	{
    private:
        int _src_roleid;
        int _dst_roleid;
        int _cs_index;
        int _cs_sid;
        gplayer* _pPlayer;
        
    public:
        CopyCallBack(int src_roleid, int dst_roleid, gplayer* pPlayer): 
            _src_roleid(src_roleid), _dst_roleid(dst_roleid), _pPlayer(pPlayer)
        {
            _cs_index = pPlayer->cs_index;
            _cs_sid = pPlayer->cs_sid;
        }

        virtual void OnTimeOut() { OnFailed();}
        virtual void OnFailed() 
        {
            printf("�����������ʧ�� src_roleid=%d, dst_roleid=%d", _src_roleid, _dst_roleid);
            delete this;
        }

        virtual void OnSucceed()
        { 
            spin_autolock keeper(_pPlayer->spinlock);
            if(_pPlayer->ID.id != _dst_roleid || !_pPlayer->IsActived()
                    || _pPlayer->cs_index != _cs_index || _pPlayer->cs_sid != _cs_sid)
            {
                delete this;
                return;
            }

            ASSERT(_pPlayer->imp);
            ((gplayer_imp*)(_pPlayer->imp))->_team.PlayerLogout();
            ((gplayer_imp*)(_pPlayer->imp))->Logout(-1); 
            
            char buff[] = {0};
            GMSV::SendDebugCommand(_dst_roleid, 31, buff, 0);
            delete this;
        }
    };
    
    int dst_roleid = _parent->ID.id;
    gplayer* pPlayer = (gplayer*)_parent;
    GDB::copy_role(src_roleid, dst_roleid, new CopyCallBack(src_roleid, dst_roleid, pPlayer));    
}

void gplayer_imp::PlayerQueryCanInheritAddons(int equip_id, unsigned int inv_idx)
{
    if(!_inventory.IsItemExist(inv_idx, equip_id, 1)) return;
    item& eq_it = _inventory[inv_idx];
	if(eq_it.body == NULL) return;

    DATA_TYPE eq_dt;
	const void* eq_ess = world_manager::GetDataMan().get_data_ptr(eq_it.type, ID_SPACE_ESSENCE, eq_dt);
	if(!eq_ess || eq_dt != DT_WEAPON_ESSENCE && eq_dt != DT_ARMOR_ESSENCE && eq_dt != DT_DECORATION_ESSENCE) return;
    
    int eq_refine_material_need = 0;
    int eq_refine_addon_id = world_manager::GetDataMan().get_item_refine_addon(eq_it.type, eq_refine_material_need);
    addon_data need_inherit_addon_list[5];
    int need_inherit_addon_count = eq_it.GetCanInheritAddon(&need_inherit_addon_list[0], sizeof(need_inherit_addon_list)/sizeof(need_inherit_addon_list[0]), eq_refine_addon_id);
    
    int addon_id_list[5];
    for (unsigned char i = 0; i < need_inherit_addon_count; ++i) {
        addon_id_list[i] = need_inherit_addon_list[i].id;
    } 

    ((gplayer_dispatcher*)_runner)->equip_can_inherit_addons(equip_id, inv_idx, need_inherit_addon_count, addon_id_list);
}

void gplayer_imp::ActivateRegionWayPoints(int num, int waypoints[])
{
    if (num <= 0) return;
    CELRegion* pRegion = city_region::GetRegion(_parent->pos.x, _parent->pos.z); 
    if (pRegion == NULL) return;

    const abase::hash_map< int, abase::vector<int> >& waypoint_map = world_manager::GetRegionWaypoints();
    abase::hash_map< int, abase::vector<int> >::const_iterator it = waypoint_map.find(pRegion->GetID());
    if (it == waypoint_map.end()) return;
    const abase::vector<int>& vec = it->second;

    for (int i = 0; i < num; ++i)
    {
        for (unsigned int j = 0; j < vec.size(); ++j)
        {
            if (waypoints[i] == vec[j])
            {
                GLog::log(GLOG_INFO,"�û�%d�����˴��͵�%d" , _parent->ID.id, waypoints[i] & 0xFFFF);
                ActivateWaypoint(waypoints[i] & 0xFFFF);
                break;
            }
        }
    }
}

void gplayer_imp::PlayerReenterInstance()
{
	if(!_player_instance_reenter.TryReenter(this))
	{
		_runner->error_message(S2C::ERR_INSTANCE_REENTER_FAIL);
	}
}

void gplayer_imp::SetHasPVPLimitFilter(bool has_pvp_limit_filter)
{
    _player_sanctuary_check.SetHasPVPLimitFilter(has_pvp_limit_filter);
}

void gplayer_imp::PlayerStartEnterSanctuarySession()
{
    _player_sanctuary_check.OnStartEnterSanctuarySession();
}

void gplayer_imp::PlayerAddPVPLimitFilter()
{
    _filters.AddFilter(new pvp_limit_filter(this, FILTER_INDEX_PVPLIMIT));
}

void gplayer_imp::EnhanceMountSpeedEn(float sp)
{
	_mount_speed_en += sp;
	_petman.OnMountSpeedEnChanged(this);
}

void gplayer_imp::ImpairMountSpeedEn(float sp)
{
	_mount_speed_en -= sp;
	_petman.OnMountSpeedEnChanged(this);
}
