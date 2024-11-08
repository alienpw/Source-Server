
#ifndef __GNET_AUCTIONLISTUPDATE_RE_HPP
#define __GNET_AUCTIONLISTUPDATE_RE_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"

#include "gauctionitem"

namespace GNET
{

class AuctionListUpdate_Re : public GNET::Protocol
{
	#include "auctionlistupdate_re"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		// TODO
		GLinkServer::GetInstance()->Send(localsid,this);
	}
};

};

#endif
