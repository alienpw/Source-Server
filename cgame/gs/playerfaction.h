#ifndef __ONLINEGAME_GS_PLAYER_IMP_FACTION_H__
#define __ONLINEGAME_GS_PLAYER_IMP_FACTION_H__

//���ɻ������ʵ��,�����ж���gplayer_battleground����һ��

class gplayer_factionfortress: public gplayer_imp
{
	int attack_faction; 	//�������ӵ�faction
	int defense_faction;	//�ܵ�����ʱ�Լ����ӵ�faction
public:
	gplayer_factionfortress()
	{
		attack_faction = 0;
		defense_faction = 0;
	}
private:
	static bool __GetPetAttackHook(gactive_imp * __this, const MSG & msg, attack_msg & amsg);
	static bool __GetPetEnchantHook(gactive_imp * __this, const MSG & msg,enchant_msg & emsg);
	static void __GetPetAttackFill(gactive_imp * __this, attack_msg & amsg);
	static void __GetPetEnchantFill(gactive_imp * __this, enchant_msg & emsg);
	
	void SetBattleFaction();
public:
	DECLARE_SUBSTANCE(gplayer_factionfortress);
	virtual int MessageHandler(world * pPlane ,const MSG & msg);
	virtual int ZombieMessageHandler(world * pPlane ,const MSG & msg);
	virtual void FillAttackMsg(const XID & target,attack_msg & attack,int dec_arrow);
	virtual void FillEnchantMsg(const XID & target,enchant_msg & enchant);
	virtual void PlayerEnterWorld();  
	virtual void PlayerEnterServer(); 
	virtual void PlayerLeaveServer(); 
	virtual void PlayerLeaveWorld();
	virtual int GetFaction();
	virtual int GetEnemyFaction();
	virtual attack_judge GetPetAttackHook();
	virtual enchant_judge GetPetEnchantHook();
	virtual attack_fill GetPetAttackFill();
	virtual enchant_fill GetPetEnchantFill();
};


#endif
