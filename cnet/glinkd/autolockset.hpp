
#ifndef __GNET_AUTOLOCKSET_HPP
#define __GNET_AUTOLOCKSET_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"


namespace GNET
{

class AutolockSet : public GNET::Protocol
{
	#include "autolockset"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		if (!GLinkServer::ValidRole(sid,roleid) ) 
			return;
		localsid = sid;
		GDeliveryClient::GetInstance()->SendProtocol(this);
	}
};

};

#endif