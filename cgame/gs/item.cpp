#include "string.h"
#include "world.h"
#include "item.h"
#include "item_list.h"
#include "clstab.h"
#include "player_imp.h"
#include <common/protocol.h>
#include "template/itemdataman.h"
#include "global_controller.h"

DEFINE_SUBSTANCE_ABSTRACT(item_body,substance,CLS_ITEM)

DEFINE_SUBSTANCE(gmatter_item_imp,gmatter_item_base_imp,CLS_MATTER_ITEM_IMP)
DEFINE_SUBSTANCE(gmatter_item_controller,gmatter_controller,CLS_MATTER_ITEM_CONTROLLER)

bool item::LoadBody(archive &ar,int classid)
{
	ASSERT(body == NULL);
	substance * pObj = substance::CreateInstance(classid);
	if(!pObj || !pObj->GetRunTimeClass()->IsDerivedFrom(CLASSINFO(item_body)))
	{
		__PRINTF("�����item class id :%d\n",classid);
		ASSERT(false && "�����ʵ���Ʒ����");
		delete pObj;
		return false;
	}
	item_body * pBody = (item_body*)pObj;
	try
	{
		pBody->_tid = type;
		if(!pBody->Load(ar))
		{
			__PRINTF("�������Ʒ���� id��%d\n",type);
			delete pBody;
			return false;
		}
	}
	catch(...)
	{
		delete pBody;
		return false;
	}
	body = pBody;
	return true;
}

bool item::CanActivate(item_list &list, int index, gactive_imp * obj)
{
	if(list.GetLocation() == BODY)
	{
		if(((gplayer_imp*)obj)->GetDisabledEquipMask() & (1LL << index)) return false;
	}
	if(proc_type & ITEM_PROC_TYPE_DAMAGED) return false;
	if(body)
		return body->CanActivate(list,obj);
	else
		return false;

}

bool MakeItemEntry(item& entry,const item_data & data)
{
	entry.type = data.type;
	entry.count = data.count;
	entry.pile_limit = data.pile_limit;
	entry.equip_mask = data.equip_mask;
	entry.proc_type = data.proc_type;
	entry.price = data.price;
	entry.expire_date = data.expire_date;
	entry.guid.guid1 = data.guid.guid1;
	entry.guid.guid2 = data.guid.guid2;
	if(data.classid >0)
	{
		raw_wrapper ar(data.item_content,data.content_length);
		if(!entry.LoadBody(ar,data.classid))
		{
			__PRINTF("��ȡ��Ʒʧ�� id:%d classid:%d size %d\n",data.type,data.classid,data.content_length);
			return false;
		}
	}
	else
	{
		entry.body = NULL;
	}
	
	return true;
}

bool MakeItemEntry(item& entry,const GDB::itemdata & data)
{
	item_data idata;
	idata.type = data.id;
	idata.count = data.count;
	idata.equip_mask = data.mask;
	idata.proc_type = data.proctype;
	idata.expire_date = data.expire_date;
	idata.classid = -1;
	idata.price = world_manager::GetDataMan().get_item_sell_price(data.id);
	idata.pile_limit = world_manager::GetDataMan().get_item_pile_limit(data.id);
	int new_proctype = world_manager::GetDataMan().get_item_proc_type(data.id);
	if((new_proctype & item::ITEM_PROC_TYPE_BIND2) && 
			!(idata.proc_type & (item::ITEM_PROC_TYPE_BIND2|item::ITEM_PROC_TYPE_BIND)))
	{
		idata.proc_type |= item::ITEM_PROC_TYPE_BIND2;
	}
	idata.proc_type &= ~item::ITEM_PROC_TYPE_NOPUTIN_USERTRASH;
	idata.proc_type |= (new_proctype & item::ITEM_PROC_TYPE_NOPUTIN_USERTRASH);

	if(((int)idata.pile_limit) <= 0 || !idata.count) 
	{
		__PRINTF("��������²�����ִ˴��󣬳�����Ʒ��ɾ������");
		return false;
	}
	if(data.expire_date == 0 && world_manager::IsExpireItem(data.id))
	{
		return false;
	}
	if(idata.count > idata.pile_limit) idata.count = idata.pile_limit;
	idata.guid.guid1 = data.guid1;
	idata.guid.guid2 = data.guid2;
	idata.item_content = (char*)data.data;
	idata.content_length = data.size;

	world_manager::GetDataMan().reset_classid(&idata);
	return MakeItemEntry(entry,idata);

}

/*
 * 	��������Ʒ�����ʵ��
 */

#include "item/equip_item.h"
void
gmatter_item_imp::SetData(const item_data & data)
{
	ASSERT(!_data);
	_data = DupeItem(data);
}

void
gmatter_item_imp::AttachData(item_data *data)
{
	ASSERT(!_data);
	ASSERT(!data->content_length || data->item_content == ((char*)data) + sizeof(item_data));
	_data = data;
}

gmatter_item_imp::~gmatter_item_imp()
{
	if(_data) FreeItem(_data);
}


void 
gmatter_item_imp::OnPickup(const XID & who,int team_id,bool is_team)
{
	int drop_id = _drop_user;
	if(team_id) drop_id |= 0x80000000;
	MSG  msg;
	BuildMessage(msg,GM_MSG_PICKUP_ITEM,who,_parent->ID,
			_parent->pos,drop_id,_data,sizeof(item_data) + _data->content_length);
	_plane->PostLazyMessage(msg);

}

int 
gmatter_item_imp::MessageHandler(world * pPlane ,const MSG & msg)
{
	switch(msg.message)
	{
		case GM_MSG_PICKUP:
			if(msg.content_length == sizeof(msg_pickup_t))
			{
				msg_pickup_t * mpt = (msg_pickup_t*)msg.content;
				Pickup<0>(msg.source,msg.param,mpt->team_seq,msg.pos, mpt->who,true);
			}
			else
			{
				ASSERT(false);
			}
			return 0;
		case GM_MSG_FORCE_PICKUP:
			if(msg.content_length == sizeof(XID))
			{
				Pickup<0>(msg.source,msg.param,0,msg.pos,*(XID*)msg.content,false);
			}
			return 0;
		default:
			return gmatter_item_base_imp::MessageHandler(pPlane,msg);
	}
}

void DropItemFromData(world *pPlane,const A3DVECTOR &pos, const item_data & data,const XID &owner,int owner_team,int seq,int drop_id)
{	
	//��������ʱ�� �������ɼ�������ҽ��е��õģ�������Ʒ����Ҳ���������Ķ�������������֣�
	//��˲����������
	gmatter * matter = pPlane->AllocMatter();
	if(matter == NULL) return ;
	matter->SetActive();
	matter->pos = pos;
	matter->ID.type = GM_TYPE_MATTER;
	matter->ID.id= MERGE_ID<gmatter>(MKOBJID(world_manager::GetWorldIndex(),pPlane->GetMatterIndex(matter)));
	matter->SetDirUp(0,0,abase::Rand(0,255));
	gmatter_item_imp *imp = new gmatter_item_imp();
	if(data.expire_date)
	{
		//���ڹ���ʱ��
		int t = data.expire_date - g_timer.get_systime();
		if(t <= 0) t = 1;
		if(t < imp->GetLife())
		{
			imp->SetLife(t);
		}
	}
	imp->SetOwner(owner,owner_team,seq);
	imp->SetData(data);
	imp->SetDrop(drop_id);
	imp->Init(pPlane,matter);
	matter->imp = imp;
	imp->_runner = new gmatter_dispatcher();
	imp->_runner->init(imp);
	imp->_commander = new gmatter_item_controller();
	imp->_commander->Init(imp);
	
	pPlane->InsertMatter(matter);
	imp->_runner->enter_world();
	matter->Unlock();
}

void DropItemData(world * pPlane,const A3DVECTOR &pos, item_data *data,const XID &owner,int owner_team,int seq,int life)
{
	//��������ʱ�� �������ɼ�������ҽ��е��õģ�������Ʒ����Ҳ���������Ķ�������������֣�
	//��˲����������
	gmatter * matter = pPlane->AllocMatter();
	if(matter == NULL) 
	{
		//�޷��������ͷŸö���
		FreeItem(data);
		return ;
	}
	matter->SetActive();
	matter->pos = pos;
	matter->ID.type = GM_TYPE_MATTER;
	matter->ID.id= MERGE_ID<gmatter>(MKOBJID(world_manager::GetWorldIndex(),pPlane->GetMatterIndex(matter)));
	matter->SetDirUp(0,0,abase::Rand(0,255));
	gmatter_item_imp *imp = new gmatter_item_imp();
	if(life) imp->SetLife(life);
	if(data->expire_date)
	{
		//���ڹ���ʱ��
		int t = data->expire_date - g_timer.get_systime();
		if(t <= 0) t = 1;
		if(t < imp->GetLife())
		{
			imp->SetLife(t);
		}
	}
	imp->SetOwner(owner,owner_team,seq);
	imp->AttachData(data);
	imp->Init(pPlane,matter);
	matter->imp = imp;
	imp->_runner = new gmatter_dispatcher();
	imp->_runner->init(imp);
	imp->_commander = new gmatter_item_controller();
	imp->_commander->Init(imp);
	
	pPlane->InsertMatter(matter);
	imp->_runner->enter_world();
	matter->Unlock();
}

int GetItemRealTimePrice(const item & it)
{
	ASSERT(it.type > 0);
	DATA_TYPE dt = world_manager::GetDataMan().get_data_type(it.type, ID_SPACE_ESSENCE);
	switch(dt)
	{
		case DT_MONEY_CONVERTIBLE_ESSENCE:
		{
			int r = world_manager::GetGlobalController().GetCashMoneyExchangeRate();
			float p = (float)it.price * r;
			if(p < 0 || p > 2e9)
			{
				return 0;
			}
			return it.price * r;
		}

		default:
			break;
	}
	return it.price;
}
bool IsItemForbidShop(int type)
{
	ASSERT(type > 0);
    if(world_manager::GetGlobalController().CheckServerForbid(SERVER_FORBID_SHOPITEM, type))
    {
        return true; 
    }

	DATA_TYPE dt = world_manager::GetDataMan().get_data_type(type, ID_SPACE_ESSENCE);
	switch(dt)
	{
		case DT_MONEY_CONVERTIBLE_ESSENCE:
		{
			return !world_manager::GetGlobalController().GetCashMoneyExchangeOpen();	
		}

		default:
			return false;
	}
}
bool IsItemForbidSell(int type)
{
	ASSERT(type > 0);
	DATA_TYPE dt = world_manager::GetDataMan().get_data_type(type, ID_SPACE_ESSENCE);
	switch(dt)
	{
		case DT_MONEY_CONVERTIBLE_ESSENCE:
		{
			return !world_manager::GetGlobalController().GetCashMoneyExchangeOpen();	
		}

		default:
			return false;
	}
}