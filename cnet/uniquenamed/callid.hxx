
#ifndef __GNET_UNIQUENAMED_CALLID
#define __GNET_UNIQUENAMED_CALLID

namespace GNET
{

enum CallID
{
	RPC_PRECREATEROLE	=	3037,
	RPC_POSTCREATEROLE	=	3038,
	RPC_POSTDELETEROLE	=	3039,
	RPC_PRECREATEFACTION	=	3040,
	RPC_POSTCREATEFACTION	=	3041,
	RPC_POSTDELETEFACTION	=	3042,
	RPC_ROLENAMEEXISTS	=	3413,
	RPC_USERROLECOUNT	=	3414,
	RPC_MOVEROLECREATE	=	3415,
	RPC_PRECREATEFAMILY	=	3046,
	RPC_POSTCREATEFAMILY	=	3049,
	RPC_POSTDELETEFAMILY	=	3050,
	RPC_DBRAWREAD	=	3055,
	RPC_PREPLAYERRENAME	=	3061,
	RPC_PREFACTIONRENAME	=	4529,
};

enum ProtocolType
{
	PROTOCOL_KEEPALIVE	=	90,
	PROTOCOL_POSTPLAYERRENAME	=	3063,
	PROTOCOL_POSTFACTIONRENAME	=	4531,
};

};
#endif