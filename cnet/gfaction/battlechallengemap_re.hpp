
#ifndef __GNET_BATTLECHALLENGEMAP_RE_HPP
#define __GNET_BATTLECHALLENGEMAP_RE_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"

#include "gbattlechallenge"

namespace GNET
{

class BattleChallengeMap_Re : public GNET::Protocol
{
	#include "battlechallengemap_re"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		// TODO
		DEBUG_PRINT("battlechallengemap_re roleid %d, status %d", roleid, status);
		GFactionServer* gfs = GFactionServer::GetInstance();
		GFactionServer::Player player;
		if(gfs->GetPlayer(roleid,player))
		{
			localsid = player.localsid;
			if(player.faction_id)
			{
				maxbonus = Factiondb::GetInstance()->GetMaxbonus(player.faction_id);
				//�˴�if��Ϊ�˽������Ҳ�ܿ�����ս��������⣬������ǰ������򽫾�����������Ϊ-1(��ʾ�а��ɾ���)
				if(player.froleid != _R_MASTER)
				{
					std::vector<GBattleChallenge>::iterator gbi=cities.begin();
					for(;gbi!=cities.end();gbi++)
					{
						if(gbi->challenger == player.faction_id)
						{
							gbi->challenger=(unsigned int)-1;
						}
					}
				}
			}
			else
				maxbonus = 0;
			gfs->DispatchProtocol(player.linkid, this);
			return;
		}
	}
};

};

#endif