#ifndef __CPPGEN_GNET_SKILL2689
#define __CPPGEN_GNET_SKILL2689
namespace GNET
{
#ifdef _SKILL_SERVER
    class Skill2689:public Skill
    {
      public:
        enum
        { SKILL_ID = 2689 };
          Skill2689 ():Skill (SKILL_ID)
        {
        }
    };
#endif
    class Skill2689Stub:public SkillStub
    {
      public:
        Skill2689Stub ():SkillStub (2689)
        {
            cls = 11;
            name = L"ˮ��ͨ";
            nativename = "ˮ��ͨ";
            icon = "ˮ��ͨ��.dds";
            max_level = 10;
            type = 5;
            apcost = 0;
            arrowcost = 0;
            apgain = 0;
            attr = 0;
            rank = 3;
            eventflag = 1;
            posdouble = 0;
            clslimit = 0;
            time_type = 0;
            showorder = 1101;
            allow_land = 1;
            allow_air = 1;
            allow_water = 1;
            allow_ride = 0;
            auto_attack = 0;
            long_range = 0;
            restrict_corpse = 0;
            allow_forms = 0;
            effect = "";
            range.type = 0;
            doenchant = false;
            dobless = false;
            commoncooldown = 0;
            commoncooldowntime = 0;
            pre_skills.push_back (std::pair < ID, int >(0, 1));
#ifdef _SKILL_SERVER
#endif
        }
        virtual ~ Skill2689Stub ()
        {
        }
        float GetMpcost (Skill * skill) const
        {
            return (float) (0);
        }
        int GetExecutetime (Skill * skill) const
        {
            return 0;
        }
        int GetCoolingtime (Skill * skill) const
        {
            return 0;
        }
        int GetRequiredLevel (Skill * skill) const
        {
            static int array[10] = { 29, 34, 39, 44, 49, 54, 59, 64, 69, 74 };
            return array[skill->GetLevel () - 1];
        }
        int GetRequiredSp (Skill * skill) const
        {
            static int array[10] = { 10738, 14878, 19926, 25992, 33370, 42240, 53070, 66080, 81600, 129060 };
            return array[skill->GetLevel () - 1];
        }
        int GetRequiredMoney (Skill * skill) const
        {
            static int array[10] = { 1090, 1540, 1990, 2480, 2980, 3480, 3980, 12980, 51980, 151980 };
            return array[skill->GetLevel () - 1];
        }
        float GetRadius (Skill * skill) const
        {
            return (float) (0);
        }
        float GetAttackdistance (Skill * skill) const
        {
            return (float) (0);
        }
        float GetAngle (Skill * skill) const
        {
            return (float) (1 - 0.0111111 * (0));
        }
        float GetPraydistance (Skill * skill) const
        {
            return (float) (0);
        }
#ifdef _SKILL_CLIENT
        int GetIntroduction (Skill * skill, wchar_t * buffer, int length, wchar_t * format) const
        {
            return _snwprintf (buffer, length, format, skill->GetLevel (), 2 * skill->GetLevel ());

        }
#endif
#ifdef _SKILL_SERVER
        int GetEnmity (Skill * skill) const
        {
            return 0;
        }
        bool TakeEffect (Skill * skill) const
        {
            skill->GetPlayer ()->SetIncwater (0.02 * skill->GetLevel ());
            return true;
        }
        float GetEffectdistance (Skill * skill) const
        {
            return (float) (0);
        }
        float GetHitrate (Skill * skill) const
        {
            return (float) (1.0);
        }
#endif
    };
}
#endif