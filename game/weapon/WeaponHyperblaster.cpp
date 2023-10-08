#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Weapon.h"

const int HYPERBLASTER_SPARM_BATTERY = 6;
const int HYPERBLASTER_SPIN_SPEED	 = 300;

class rvWeaponHyperblaster : public rvWeapon {
public:

	CLASS_PROTOTYPE( rvWeaponHyperblaster );

	rvWeaponHyperblaster ( void );

	virtual void			Spawn				( void );
	void					Save				( idSaveGame *savefile ) const;
	void					Restore				( idRestoreGame *savefile );
	void					PreSave				( void );
	void					PostSave			( void );

protected:

	jointHandle_t			jointBatteryView;
	bool					spinning;

	void					SpinUp				( void );
	void					SpinDown			( void );

private:

	stateResult_t		State_Idle		( const stateParms_t& parms );
	stateResult_t		State_Fire		( const stateParms_t& parms );
	stateResult_t		State_Reload	( const stateParms_t& parms );

	int					chargeTime;
	int					chargeDelay;
	idVec2				chargeGlow;
	bool				fireForced;
	int					fireHeldTime;

	bool				UpdateAttack(void);

	stateResult_t		State_Charge(const stateParms_t& parms);
	stateResult_t		State_Charged(const stateParms_t& parms);

	bool				missile;
	
	CLASS_STATES_PROTOTYPE ( rvWeaponHyperblaster );
};

CLASS_DECLARATION( rvWeapon, rvWeaponHyperblaster )
END_CLASS

/*
================
rvWeaponHyperblaster::rvWeaponHyperblaster
================
*/
rvWeaponHyperblaster::rvWeaponHyperblaster ( void ) {
}

/*
================
rvWeaponHyperblaster::Spawn
================
*/
void rvWeaponHyperblaster::Spawn ( void ) {
	jointBatteryView = viewAnimator->GetJointHandle ( spawnArgs.GetString ( "joint_view_battery" ) );
	spinning		 = false;
	
	chargeGlow = spawnArgs.GetVec2("chargeGlow");
	chargeTime = SEC2MS(spawnArgs.GetFloat("chargeTime"));
	chargeDelay = SEC2MS(spawnArgs.GetFloat("chargeDelay"));

	fireHeldTime = 0;
	fireForced = false;

	missile = false;

	SetState ( "Raise", 0 );	
}

/*
================
rvWeaponHyperblaster::Save
================
*/
void rvWeaponHyperblaster::Save ( idSaveGame *savefile ) const {
	savefile->WriteJoint ( jointBatteryView );
	savefile->WriteBool ( spinning );
	savefile->WriteInt(chargeTime);
	savefile->WriteInt(chargeDelay);
	savefile->WriteVec2(chargeGlow);
	savefile->WriteBool(fireForced);
	savefile->WriteInt(fireHeldTime);
}

/*
================
rvWeaponHyperblaster::Restore
================
*/
void rvWeaponHyperblaster::Restore ( idRestoreGame *savefile ) {
	savefile->ReadJoint ( jointBatteryView );
	savefile->ReadBool ( spinning );
	savefile->ReadInt(chargeTime);
	savefile->ReadInt(chargeDelay);
	savefile->ReadVec2(chargeGlow);
	savefile->ReadBool(fireForced);
	savefile->ReadInt(fireHeldTime);
}

/*
================
rvWeaponHyperBlaster::PreSave
================
*/
void rvWeaponHyperblaster::PreSave ( void ) {

	SetState ( "Idle", 4 );

	StopSound( SND_CHANNEL_WEAPON, false );
	StopSound( SND_CHANNEL_BODY, false );
	StopSound( SND_CHANNEL_ITEM, false );
	StopSound( SND_CHANNEL_ANY, false );
	
}

/*
================
rvWeaponHyperBlaster::PostSave
================
*/
void rvWeaponHyperblaster::PostSave ( void ) {
}

/*
================
rvWeaponHyperblaster::SpinUp
================
*/
void rvWeaponHyperblaster::SpinUp ( void ) {
	if ( spinning ) {
		return;
	}
	
	if ( jointBatteryView != INVALID_JOINT ) {
		viewAnimator->SetJointAngularVelocity ( jointBatteryView, idAngles(0,HYPERBLASTER_SPIN_SPEED,0), gameLocal.time, 50 );
	}

	StopSound ( SND_CHANNEL_BODY2, false );
	StartSound ( "snd_battery_spin", SND_CHANNEL_BODY2, 0, false, NULL );
	spinning = true;
}

/*
================
rvWeaponHyperblaster::SpinDown
================
*/
void rvWeaponHyperblaster::SpinDown	( void ) {
	if ( !spinning ) {
		return;
	}
	
	StopSound ( SND_CHANNEL_BODY2, false );
	StartSound ( "snd_battery_spindown", SND_CHANNEL_BODY2, 0, false, NULL );

	if ( jointBatteryView != INVALID_JOINT ) {
		viewAnimator->SetJointAngularVelocity ( jointBatteryView, idAngles(0,0,0), gameLocal.time, 500 );
	}

	spinning = false;
}

bool rvWeaponHyperblaster::UpdateAttack(void) {
	// Clear fire forced

	if (fireForced) {
		if (!(wsfl.attack || wsfl.missile)) {
			fireForced = false;
		}
		else {
			return false;
		}
	}

	// If the player is pressing the fire button and they have enough ammo for a shot
	// then start the shooting process.
	if ((wsfl.attack || wsfl.missile) && gameLocal.time >= nextAttackTime) {
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

CLASS_STATES_DECLARATION ( rvWeaponHyperblaster )
	STATE ( "Idle",				rvWeaponHyperblaster::State_Idle)
	STATE ( "Fire",				rvWeaponHyperblaster::State_Fire )
	STATE ( "Reload",			rvWeaponHyperblaster::State_Reload )
	STATE ( "Charge",			rvWeaponHyperblaster::State_Charge )
	STATE ( "Charged",			rvWeaponHyperblaster::State_Charged )
END_CLASS_STATES

/*
================
rvWeaponHyperblaster::State_Idle
================
*/
stateResult_t rvWeaponHyperblaster::State_Idle( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			if ( !AmmoAvailable ( ) ) {
				SetStatus ( WP_OUTOFAMMO );
			} else {
				SetStatus ( WP_READY );
			}

			SpinDown ( );

			if ( ClipSize() ) {
				viewModel->SetShaderParm ( HYPERBLASTER_SPARM_BATTERY, (float)AmmoInClip()/ClipSize() );
			} else {
				viewModel->SetShaderParm ( HYPERBLASTER_SPARM_BATTERY, 1.0f );		
			}
			PlayCycle( ANIMCHANNEL_ALL, "idle", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_WAIT );
		
		case STAGE_WAIT:			
			if ( wsfl.lowerWeapon ) {
				SetState ( "Lower", 4 );
				return SRESULT_DONE;
			}

			if (UpdateAttack()) {
				return SRESULT_DONE;
			}
			/*
			if ( !clipSize ) {
				if ( gameLocal.time > nextAttackTime && wsfl.attack && AmmoAvailable ( ) ) {
					SetState ( "Fire", 0 );
					return SRESULT_DONE;
				} 
			} else {
				if ( gameLocal.time > nextAttackTime && wsfl.attack && AmmoInClip ( ) ) {
					SetState ( "Fire", 0 );
					return SRESULT_DONE;
				}  
				if ( wsfl.attack && AutoReload() && !AmmoInClip ( ) && AmmoAvailable () ) {
					SetState ( "Reload", 4 );
					return SRESULT_DONE;			
				}
				if ( wsfl.netReload || (wsfl.reload && AmmoInClip() < ClipSize() && AmmoAvailable()>AmmoInClip()) ) {
					SetState ( "Reload", 4 );
					return SRESULT_DONE;			
				}				
			}
			*/

			if (AutoReload() && !AmmoInClip() && AmmoAvailable()) {
				SetState("Reload", 2);
				return SRESULT_DONE;
			}
			if (wsfl.netReload || (wsfl.reload && AmmoInClip() < ClipSize() && AmmoAvailable() > AmmoInClip())) {
				SetState("Reload", 4);
				return SRESULT_DONE;
			}

			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponHyperblaster::State_Fire
================
*/
stateResult_t rvWeaponHyperblaster::State_Fire ( const stateParms_t& parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};	
	switch ( parms.stage ) {
		case STAGE_INIT:
			//SpinUp();

			idPlayer* player;
			player = gameLocal.GetLocalPlayer();

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
			/*
			nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier ( PMOD_FIRERATE ));
			Attack ( false, 1, spread, 0, 1.0f );
			if ( ClipSize() ) {
				viewModel->SetShaderParm ( HYPERBLASTER_SPARM_BATTERY, (float)AmmoInClip()/ClipSize() );
			} else {
				viewModel->SetShaderParm ( HYPERBLASTER_SPARM_BATTERY, 1.0f );		
			}
			PlayAnim ( ANIMCHANNEL_ALL, "fire", 0 );	
			*/

			if (missile)
			{
				if (gameLocal.time - fireHeldTime > chargeTime)
				{
					//3 means Super Missile
					Attack(3, 1, 1, 0, 4.0f);
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
					Attack(true, 3, 3, 0, 1.5f);
					PlayEffect("fx_muzzleflash", barrelJointView, false);
					PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
				}
				else
				{
					//gameLocal.Printf("Regular Shot!\n");
					Attack(false, 1, spread, 0, 1.0f);
					PlayEffect("fx_muzzleflash", barrelJointView, false);
					PlayAnim(ANIMCHANNEL_ALL, "fire", parms.blendFrames);
				}
			}

			fireHeldTime = 0;

			return SRESULT_STAGE ( STAGE_WAIT );
	
		case STAGE_WAIT:		
			/*
			if ( wsfl.attack && gameLocal.time >= nextAttackTime && AmmoInClip() && !wsfl.lowerWeapon ) {
				SetState ( "Fire", 0 );
				return SRESULT_DONE;
			}
			if ( (!wsfl.attack || !AmmoInClip() || wsfl.lowerWeapon) && AnimDone ( ANIMCHANNEL_ALL, 0 ) ) {
				SetState ( "Idle", 0 );
				return SRESULT_DONE;
			}
			*/
			
			if ((gameLocal.isMultiplayer && gameLocal.time >= nextAttackTime) ||
				(!gameLocal.isMultiplayer && (AnimDone(ANIMCHANNEL_ALL, 60)))) {
				SetState("Idle", 0);
				return SRESULT_DONE;
			}

			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponHyperblaster::State_Reload
================
*/
stateResult_t rvWeaponHyperblaster::State_Reload ( const stateParms_t& parms ) {
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
			
			SpinDown ( );

			viewModel->SetShaderParm ( HYPERBLASTER_SPARM_BATTERY, 0 );
			
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
stateResult_t rvWeaponHyperblaster::State_Charge(const stateParms_t& parms) {
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
			//viewModel->PlayEffect("fx_muzzleflash", flashJointView, false, vec3_origin, false, EC_IGNORE, muzzleTint ? owner->GetHitscanTint() : vec4_one);
			PlayEffect("fx_muzzleflash", barrelJointView, false);

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
stateResult_t rvWeaponHyperblaster::State_Charged(const stateParms_t& parms) {
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
		//viewModel->PlayEffect("fx_muzzleflashCharged", flashJointView, false, vec3_origin, false, EC_IGNORE, muzzleTint ? owner->GetHitscanTint() : vec4_one);
		PlayEffect("fx_muzzleflash_charged", barrelJointView, false);

		if (!(wsfl.attack || wsfl.missile)) {
			fireForced = true;
			SetState("Fire", 0);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}