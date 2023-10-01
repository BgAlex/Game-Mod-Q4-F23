#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Weapon.h"

const idEventDef EV_Railgun_RestoreHum( "<railgunRestoreHum>", "" );

class rvWeaponRailgun : public rvWeapon {
public:

	CLASS_PROTOTYPE( rvWeaponRailgun );

	rvWeaponRailgun ( void );

	virtual void			Spawn				( void );
	virtual void			Think				( void );
	void					Save				( idSaveGame *savefile ) const;
	void					Restore				( idRestoreGame *savefile );
	void					PreSave				( void );
	void					PostSave			( void );
	void					ClientUnstale		( void );

protected:
	jointHandle_t			jointBatteryView;

private:

	stateResult_t		State_Idle		( const stateParms_t& parms );
	stateResult_t		State_Fire		( const stateParms_t& parms );
	stateResult_t		State_Reload	( const stateParms_t& parms );

	void				Event_RestoreHum	( void );

	int					chargeTime;
	int					chargeDelay;
	idVec2				chargeGlow;
	bool				fireForced;
	int					fireHeldTime;

	bool				UpdateAttack		( void );

	stateResult_t		State_Charge(const stateParms_t& parms);
	stateResult_t		State_Charged(const stateParms_t& parms);

	bool				missile;

	CLASS_STATES_PROTOTYPE ( rvWeaponRailgun );
};

CLASS_DECLARATION( rvWeapon, rvWeaponRailgun )
	EVENT( EV_Railgun_RestoreHum,			rvWeaponRailgun::Event_RestoreHum )
END_CLASS

/*
================
rvWeaponRailgun::rvWeaponRailgun
================
*/
rvWeaponRailgun::rvWeaponRailgun ( void ) {
}

/*
================
rvWeaponRailgun::Spawn
================
*/
void rvWeaponRailgun::Spawn ( void ) {
	SetState ( "Raise", 0 );

	chargeGlow = spawnArgs.GetVec2("chargeGlow");
	chargeTime = SEC2MS(spawnArgs.GetFloat("chargeTime"));
	chargeDelay = SEC2MS(spawnArgs.GetFloat("chargeDelay"));

	fireHeldTime = 0;
	fireForced = false;

	missile = false;

}

/*
================
rvWeaponRailgun::Save
================
*/
void rvWeaponRailgun::Save ( idSaveGame *savefile ) const {
	savefile->WriteJoint( jointBatteryView );
	savefile->WriteInt(chargeTime);
	savefile->WriteInt(chargeDelay);
	savefile->WriteVec2(chargeGlow);
	savefile->WriteBool(fireForced);
	savefile->WriteInt(fireHeldTime);
}

/*
================
rvWeaponRailgun::Restore
================
*/
void rvWeaponRailgun::Restore ( idRestoreGame *savefile ) {
	savefile->ReadJoint( jointBatteryView );
	savefile->ReadInt(chargeTime);
	savefile->ReadInt(chargeDelay);
	savefile->ReadVec2(chargeGlow);
	savefile->ReadBool(fireForced);
	savefile->ReadInt(fireHeldTime);
}

/*
================
rvWeaponRailgun::PreSave
================
*/
void rvWeaponRailgun::PreSave ( void ) {

	//this should shoosh the humming but not the shooting sound.
	StopSound( SND_CHANNEL_BODY2, 0);
}

/*
================
rvWeaponRailgun::PostSave
================
*/
void rvWeaponRailgun::PostSave ( void ) {

	//restore the humming
	PostEventMS( &EV_Railgun_RestoreHum, 10);
}

/*
================
rvWeaponRailgun::Think
================
*/
void rvWeaponRailgun::Think ( void ) {

	// Let the real weapon think first
	rvWeapon::Think ( );

	/*
	if ( zoomGui && wsfl.zoom && !gameLocal.isMultiplayer ) {
		int ammo = AmmoInClip();
		if ( ammo >= 0 ) {
			zoomGui->SetStateInt( "player_ammo", ammo );
		}			
	}
	*/


}


bool rvWeaponRailgun::UpdateAttack(void) {
	// Clear fire forced
	
	if (fireForced) {
		if (!( wsfl.attack || wsfl.missile) ) {
			fireForced = false;
		}
		else {
			return false;
		}
	}

	// If the player is pressing the fire button and they have enough ammo for a shot
	// then start the shooting process.
	//gameLocal.Printf("POWER BEAM: gameLocal.time: %f\tnexAttackTime: %f\n", gameLocal.time, nextAttackTime);
	if ( (wsfl.attack || wsfl.missile) && gameLocal.time >= nextAttackTime ) {
		missile = wsfl.missile;
		// Save the time which the fire button was pressed
		if (fireHeldTime == 0) {
			nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier(PMOD_FIRERATE));
			fireHeldTime = gameLocal.time;
			viewModel->SetShaderParm(6, chargeGlow[0]);
		}
	}

	// If they have the charge mod and they have overcome the initial charge 
	// delay then transition to the charge state.
	if (fireHeldTime != 0) {
		if (gameLocal.time - fireHeldTime > chargeDelay) {
			SetState("Charge", 4);
			return true;
		}


		// If the fire button was let go but was pressed at one point then 
		// release the shot.
		if (!(wsfl.attack || wsfl.missile)) {
			idPlayer* player = gameLocal.GetLocalPlayer();
			if (player) {

				if (player->GuiActive()) {
					//make sure the player isn't looking at a gui first
					SetState("Lower", 0);
				}
				else {
					SetState("Fire", 0);
				}
			}
			return true;
		}
	}

	return false;
}


/*
===============================================================================

	States 

===============================================================================
*/

CLASS_STATES_DECLARATION ( rvWeaponRailgun )
	STATE ( "Idle",				rvWeaponRailgun::State_Idle)
	STATE ( "Fire",				rvWeaponRailgun::State_Fire )
	STATE ( "Reload",			rvWeaponRailgun::State_Reload )
	STATE ( "Charge",				rvWeaponRailgun::State_Charge)
	STATE ( "Charged",			rvWeaponRailgun::State_Charged)
END_CLASS_STATES

/*
================
rvWeaponRailgun::State_Idle
================
*/
stateResult_t rvWeaponRailgun::State_Idle( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			if ( !AmmoAvailable ( ) ) {
				SetStatus ( WP_OUTOFAMMO );
			} else {
				StopSound( SND_CHANNEL_BODY2, false );
				StartSound( "snd_idle_hum", SND_CHANNEL_BODY2, 0, false, NULL );
				SetStatus ( WP_READY );
			}
			PlayCycle( ANIMCHANNEL_ALL, "idle", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_WAIT );
		
		case STAGE_WAIT:			
			if ( wsfl.lowerWeapon ) {
				StopSound( SND_CHANNEL_BODY2, false );
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}
			/*
			if ( gameLocal.time > nextAttackTime && wsfl.attack && AmmoInClip ( ) ) {
				SetState ( "Fire", 0 );
				return SRESULT_DONE;
			} 
			*/

			if (UpdateAttack()) {
				return SRESULT_DONE;
			}

			// Auto reload?
			if ( AutoReload() && !AmmoInClip ( ) && AmmoAvailable () ) {
				SetState ( "reload", 2 );
				return SRESULT_DONE;
			}
			if ( wsfl.netReload || (wsfl.reload && AmmoInClip() < ClipSize() && AmmoAvailable()>AmmoInClip()) ) {
				SetState ( "Reload", 4 );
				return SRESULT_DONE;			
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponRailgun::State_Fire
================
*/
stateResult_t rvWeaponRailgun::State_Fire ( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};

	switch ( parms.stage ) {
		case STAGE_INIT:
			/*
			nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier ( PMOD_FIRERATE ));
			Attack ( false, 1, spread, 0, 1.0f );
			PlayAnim ( ANIMCHANNEL_ALL, "fire", 0 );	
			*/

			//STOLEN FROM BLASTER
			//don't fire if we're targeting a gui.
			idPlayer* player;
			player = gameLocal.GetLocalPlayer();

			//make sure the player isn't looking at a gui first
			if (player && player->GuiActive()) {
				fireHeldTime = 0;
				SetState("Lower", 0);
				return SRESULT_DONE;
			}

			if (player && !player->CanFire()) {
				fireHeldTime = 0;
				SetState("Idle", 4);
				return SRESULT_DONE;
			}


			//gameLocal.Printf("Fire Held Time: %d\nCharge Time: %d\n", fireHeldTime, chargeTime);
			if (missile)
			{
				if (gameLocal.time - fireHeldTime > chargeTime)
				{
					//3 means Super Missile
					Attack(3, 1, 1, 0, 15.0f);
					PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
					missile = false;
				}
				else
				{
					//2 means Regular Missile
					Attack(2, 1, 1, 0, 1.0f);
					PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
					missile = false;
				}
				
			}
			else 
			{
				if (gameLocal.time - fireHeldTime > chargeTime)
				{
					Attack(true, 1, 1, 0, 15.0f);
					//PlayEffect("fx_chargedflash", barrelJointView, false);
					PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
				}
				else 
				{
					//gameLocal.Printf("Regular Shot!\n");
					Attack(false, 1, spread, 0, 1.0f);
					//PlayEffect("fx_normalflash", barrelJointView, false);
					PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
				}
			}
			
			fireHeldTime = 0;
			// STOLEN FROM BLASTER

			return SRESULT_STAGE ( STAGE_WAIT );

			
			
		case STAGE_WAIT:		
			if ( ( gameLocal.isMultiplayer && gameLocal.time >= nextAttackTime ) || 
				 ( !gameLocal.isMultiplayer && ( AnimDone ( ANIMCHANNEL_ALL, 60 ) ) ) ) {
				SetState ( "Idle", 0 );
				return SRESULT_DONE;
			}		
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}


/*
================
rvWeaponRailgun::State_Reload
================
*/
stateResult_t rvWeaponRailgun::State_Reload ( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			if ( wsfl.netReload ) {
				wsfl.netReload = false;
			} else {
				NetReload ( );
			}
						
			SetStatus ( WP_RELOAD );
			PlayAnim ( ANIMCHANNEL_ALL, "reload", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_WAIT );
			
		case STAGE_WAIT:
			if ( AnimDone ( ANIMCHANNEL_ALL, 4 ) ) {
				AddToClip ( ClipSize() );
				SetState ( "Idle", 4 );
				return SRESULT_DONE;
			}
			if ( wsfl.lowerWeapon ) {
				StopSound( SND_CHANNEL_BODY2, false );
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponBlaster::State_Charge
================
*/
stateResult_t rvWeaponRailgun::State_Charge(const stateParms_t& parms) {
	enum {
		CHARGE_INIT,
		CHARGE_WAIT,
	};
	bool muzzleTint = spawnArgs.GetBool("muzzleTint");
	switch (parms.stage) {
	case CHARGE_INIT:
		viewModel->SetShaderParm(6, chargeGlow[0]);
		StartSound("snd_charge", SND_CHANNEL_ITEM, 0, false, NULL);
		//PlayCycle(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
		return SRESULT_STAGE(CHARGE_WAIT);

	case CHARGE_WAIT:
		if (gameLocal.time - fireHeldTime < chargeTime) {
			float f;
			f = (float)(gameLocal.time - fireHeldTime) / (float)chargeTime;
			f = chargeGlow[0] + f * (chargeGlow[1] - chargeGlow[0]);
			f = idMath::ClampFloat(chargeGlow[0], chargeGlow[1], f);
			viewModel->SetShaderParm(6, f);
			//ME ATTEMPING TO UHHHHH DO A SOMTHING.
			viewModel->PlayEffect("fx_muzzleflash", flashJointView, false, vec3_origin, false, EC_IGNORE, muzzleTint ? owner->GetHitscanTint() : vec4_one);

			if (!(wsfl.attack || wsfl.missile)) {
				SetState("Fire", 0);
				return SRESULT_DONE;
			}

			return SRESULT_WAIT;
		}
		SetState("Charged", 4);
		return SRESULT_DONE;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponBlaster::State_Charged
================
*/
stateResult_t rvWeaponRailgun::State_Charged(const stateParms_t& parms) {
	enum {
		CHARGED_INIT,
		CHARGED_WAIT,
	};

	bool muzzleTint = spawnArgs.GetBool("muzzleTint");

	switch (parms.stage) {
	case CHARGED_INIT:
		viewModel->SetShaderParm(6, 1.0f);

		StopSound(SND_CHANNEL_ITEM, false);
		StartSound("snd_charge_loop", SND_CHANNEL_ITEM, 0, false, NULL);
		StartSound("snd_charge_click", SND_CHANNEL_BODY, 0, false, NULL);
		return SRESULT_STAGE(CHARGED_WAIT);

	case CHARGED_WAIT:
		//ME ATTEMPING TO UHHHHH DO A SOMTHING.
		viewModel->PlayEffect("fx_muzzleflashCharged", flashJointView, false, vec3_origin, false, EC_IGNORE, muzzleTint ? owner->GetHitscanTint() : vec4_one);
		if (!(wsfl.attack || wsfl.missile)) {
			fireForced = true;
			SetState("Fire", 0);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
===============================================================================

	Event 

===============================================================================
*/

/*
================
rvWeaponRailgun::State_Reload
================
*/
void rvWeaponRailgun::Event_RestoreHum ( void ) {
	StopSound( SND_CHANNEL_BODY2, false );
	StartSound( "snd_idle_hum", SND_CHANNEL_BODY2, 0, false, NULL );
}

/*
================
rvWeaponRailgun::ClientUnStale
================
*/
void rvWeaponRailgun::ClientUnstale( void ) {
	Event_RestoreHum();
}

