
#ifndef __GNET_PLAYERCHANGEDS_RE_HPP
#define __GNET_PLAYERCHANGEDS_RE_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"

namespace GNET
{

class PlayerChangeDS_Re : public GNET::Protocol
{
	#include "playerchangeds_re"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		//ֻ�� retcode==ERR_SUCCESS ʱ gdeliveryd �Ż��glinkd����Э��
		GLinkServer* lsm = GLinkServer::GetInstance();
		
		if(flag == DS_TO_CENTRALDS || flag == CENTRALDS_TO_DS) {
			if(!lsm->ValidRole(localsid, roleid)) return;
			
			RoleData* uinfo = lsm->GetRoleInfo(roleid);
			if(uinfo != NULL) uinfo->status = _STATUS_ONLINE;
			
			//��ɾ roleinfo ��Ϊ��ʱ���ܿͻ��˻��ᷢ��һЩЭ����� �����������ֱ�Ӻ��� ��Ӧ��ֱ�� close session. �ͻ��� close session ���� glinkd ��ʱ 30 �� close session ʱ roleinfo ��ɾ��
			//if(!lsm->RoleLogout( localsid, roleid )) return;
			 
		} else if(flag == DIRECT_TO_CENTRALDS) {
			if(!lsm->ValidUser(localsid, userid)) return;
		} else {
			return;
		}
		
		lsm->AccumulateSend(localsid, this);
		lsm->SetReadyCloseTime(localsid, 30);
		
		LOG_TRACE("CrossRelated Send PlayerChangeDS_Re to client ret %d roleid %d remote_roleid %d userid %d flag %d random.size %d dst_zoneid %d localsid %d",
				retcode, roleid, remote_roleid, userid, flag, random.size(), dst_zoneid, localsid);
		
		//��glink��gdeliveryd�ѹ� ʹ�ÿͻ���delsession��ʱ�� ����֪ͨgdeliveryd
		//��ʱdeliveryd��Userinfo.statusΪREMOTE_HALFLOGIN״̬ ��ʱ���Զ����
		SessionInfo* sinfo = lsm->GetSessionInfo(localsid);
		if(sinfo != NULL) {
			sinfo->userid = 0;
			
			time_t now = time(NULL);
			int status = 0x0;
			char strpeer[256];
			strcpy( strpeer,inet_ntoa(((struct sockaddr_in*)sinfo->GetPeer())->sin_addr) );
			
			Log::formatlog("logout","account=%.*s:userid=%d:sid=%d:peer=%s:time=%d:status=0x%x",
					sinfo->identity.size(), (char*)sinfo->identity.begin(), userid, localsid, strpeer, now-sinfo->login_time, status);
		}
	}
};

};

#endif