#include "fpcrtl.h"

#include "uAI2.h"
#include "uLandUtils.h"
#include "uFloat.h"
#include "uVariables.h"
#include "uAmmos.h"
#include "uConsts.h"
#include "uTypes.h"
#include "uCommands.h"
#include "uUtils.h"
#include "uDebug.h"
#include "uAILandMarks.h"
#include "uGearsUtils.h"
#include "uRust.h"
#include "uConsole.h"
#include "uAIMisc.h"
static const string255 __str0 = STRINIT("");
static pointer ai;
void uai2_initiateThinking()
{
    LongWord currHedgehogIndex;
    LongWord itHedgehog;
    LongWord i;
    TAmmoType itAmmo;
    TAmmoCounts ammoCounts;
    uaimisc_FillTargets();
    ai_clear_targets(ai);
    {i = 0;
     LongWord i__end__ = ((int)(Targets.Count) - 1);
     if (i <= i__end__) do {
                               if(!Targets.ar[i].dead)
                               {
                                   ai_add_target(ai, Targets.ar[i].Point.x, Targets.ar[i].Point.y, Targets.ar[i].Score, Targets.ar[i].Radius, Targets.ar[i].Density);
                               }
                           } while(i++ != i__end__);}
    ai_clear_team(ai);
    currHedgehogIndex = CurrentTeam->CurrHedgehog;
    itHedgehog = currHedgehogIndex;
    do {
           {
               if((CurrentTeam->Hedgehogs[itHedgehog].Gear != NULL) && (CurrentTeam->Hedgehogs[itHedgehog].Effects[heFrozen] == 0))
               {
                   {itAmmo = 0;
                    TAmmoType itAmmo__end__ = 60;
                    if (itAmmo <= itAmmo__end__) do {
                                                        ammoCounts[itAmmo] = uammos_HHHasAmmo(&(CurrentTeam->Hedgehogs[itHedgehog]), itAmmo);
                                                    } while(itAmmo++ != itAmmo__end__);}
                   ai_add_team_hedgehog(ai, CurrentTeam->Hedgehogs[itHedgehog].Gear, &(ammoCounts));
               }
           }
           itHedgehog = (itHedgehog + 1) % CurrentTeam->HedgehogsNumber;
       } while(!(itHedgehog == currHedgehogIndex));
    ai_think(ai);
};
void uai2_processActions()
{
    string255 action;
    ai_get_action(ai, CurrentHedgehog->Gear, &(action));
    if(_strncompare(action, __str0))
    {
        while(fpcrtl_pos(0xa, action) > 0)
        {
            ucommands_ParseCommand_2(fpcrtl_copy(action, 1, fpcrtl_pos(0xa, action) - 1), true);
            fpcrtl_Delete(action, 1, fpcrtl_pos(0xa, action));
        }
        if(_strncompare(action, __str0))
        {
            ucommands_ParseCommand_2(action, true);
        }
    }
};
void uai2_ProcessBot()
{
    {
        if(((*CurrentHedgehog).Gear == NULL) || (((*CurrentHedgehog).Gear->State & gstHHDriven) == 0))
        {
            if(ai != NULL)
            {
                ai_clear_team(ai);
                ai_think(ai);
            }
            return;
        }
    }
    if(ai == NULL)
    {
        ai = create_ai(gameField);
    }
    if(!ai_have_plan(ai) && ((GameTicks % 128) == 0))
    {
        uai2_initiateThinking();
        return;
    }
    else
    {
        uai2_processActions();
    }
};
void uai2_initModule()
{
    ai = NULL;
};
