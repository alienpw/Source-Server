
#ifndef __GNET_POSTPLAYERRENAME_HPP
#define __GNET_POSTPLAYERRENAME_HPP

#include "rpcdefs.h"
#include "callid.hxx"
#include "state.hxx"


namespace GNET
{

class PostPlayerRename : public GNET::Protocol
{
	#include "postplayerrename"

	void Process(Manager *manager, Manager::Session::ID sid)
	{
		LOG_TRACE("PostPlayerRename(dbretcode=%d, zoneid=%d roleid=%d newnamesize=%d oldnamesize=%d)", retcode, zoneid,roleid,newname.size(),oldname.size());
		UniqueNameServer::GetInstance()->TransformName(newname);

		try
		{
			StorageEnv::Storage * punamerole = StorageEnv::GetStorage("unamerole");
			StorageEnv::CommonTransaction txn;
			try
			{
				if (retcode == 0)	//db�޸ĳɹ���
				{
					//�����Ҫ��������������Ҫ��Ȼdb���쳣�Ļ�retcode��Ϊ0��oldnameΪ��
					UniqueNameServer::GetInstance()->TransformName(oldname);
					//�������ֵ�״̬����Ϊ������
					Marshal::OctetsStream value_unamerole, value_old;
					if (punamerole->find(oldname, value_old, txn))
					{
						int old_zoneid, old_roleid, old_status, old_time;
						value_old >> old_zoneid >> old_roleid >> old_status >> old_time;
						if (old_roleid == roleid)
						{
							old_status = UNIQUENAME_OBSOLETE;
							value_old.clear();
							value_old << old_zoneid << old_roleid << old_status << old_time;
							punamerole->insert(oldname, value_old, txn);
						}
						else
							Log::log(LOG_ERR, "PostPlayerRename oldroleid %d newroleid %d not equal", old_roleid, roleid);
					}
					//�������ֵ�״̬����Ϊ��ʽʹ��
					int status = UNIQUENAME_USED;
					value_unamerole << zoneid << roleid << status << (int)Timer::GetTime();
					punamerole->insert(newname, value_unamerole, txn);
					LOG_TRACE("PostPlayerRename modify newname ok! roleid=%d",roleid);
				}
				else
				{
					punamerole->del(newname, txn);
					LOG_TRACE("PostPlayerRename erase newname ok! roleid=%d", roleid);
				}
			}
			catch ( DbException e ) { throw; }
			catch ( ... )
			{
				DbException ee( DB_OLD_VERSION );
				txn.abort( ee );
				throw ee;
			}
		}
		catch ( DbException e )
		{
			Log::log( LOG_ERR, "PostPlayerRename failed, zoneid=%d, roleid=%d, what=%s\n", zoneid, roleid, e.what() );
		}
	}
};

};

#endif