#pragma once

#define MAX_STRAFES 33
#define MAX_PLAYERS 33

struct PlayerStats {
	// Resets for every jump
    int strafes = 0;
    int sync = 0;
    float gain = 0;
    int frames = 0;
    int goodFrames = 0;
	int overlaps = 0;

	// Resets for every strafe
	int strafesFrames[MAX_STRAFES] = {0}; 
	int strafesGoodFrames[MAX_STRAFES] = {0};

    float oldSpeed = 0.0;
	vec3_t oldAngles;
	bool isStrafingRight = false;
	bool isStrafingLeft = false;
    bool wasOnGround = false;
};

PlayerStats g_playerStats[MAX_PLAYERS];
size_t g_iStrafes[MAX_PLAYERS];
size_t g_iSync[MAX_PLAYERS];
bool g_bOnGround[MAX_PLAYERS];

int g_fwStrafe;

void ResetJump(PlayerStats &stats) {
	stats.strafes = 0;
	stats.sync = 0;
	stats.gain = 0.0;
	stats.frames = 0;
	stats.goodFrames = 0;
	stats.overlaps = 0;

	memset(stats.strafesFrames, 0, sizeof(stats.strafesFrames));
	memset(stats.strafesGoodFrames, 0, sizeof(stats.strafesGoodFrames));

	stats.oldAngles;
	stats.isStrafingRight = false;
    stats.isStrafingLeft = false;
	stats.wasOnGround = false;
}

void HandleStrafing(PlayerStats &stats, vec3_t ang, vec3_t vel, int buttons) {
	bool isTurning = ang.y != stats.oldAngles.y;

	if (!isTurning){
        return;
    }

    float speed = sqrt(vel.x * vel.x + vel.y * vel.y);
	stats.gain += (speed - stats.oldSpeed);

	if (!stats.isStrafingLeft && (buttons & (IN_FORWARD | IN_MOVELEFT)) && !(buttons & (IN_BACK | IN_MOVERIGHT))) {
		stats.isStrafingRight = false;
        stats.isStrafingLeft = true;

		stats.strafes++;
	} else if (!stats.isStrafingRight && (buttons & (IN_BACK | IN_MOVERIGHT)) && !(buttons & (IN_FORWARD | IN_MOVELEFT))) {
		stats.isStrafingRight = true;
        stats.isStrafingLeft = false;

		stats.strafes++;
	}

	if (speed > stats.oldSpeed) {
		stats.goodFrames++;
        if(stats.strafes && stats.strafes < MAX_STRAFES)
	        stats.strafesGoodFrames[stats.strafes - 1]++;
	}

	stats.frames++;
    if(stats.strafes && stats.strafes < MAX_STRAFES)
	    stats.strafesFrames[stats.strafes - 1]++;

	stats.oldSpeed = speed;
	stats.oldAngles = ang;
		
}

void CalculateStrafes(struct playermove_s *pMove, int player) {
	PlayerStats &stats = g_playerStats[player];

	vec3_t ang = pMove->angles;
	vec3_t vel = pMove->velocity;
	int buttons = pMove->oldbuttons;
	bool onGround = pMove->flags & FL_ONGROUND;
	
	g_bOnGround[player] = onGround;

	if(buttons & IN_MOVERIGHT && buttons & IN_MOVELEFT)
		stats.overlaps++;

	if (!onGround) { // In Air
		HandleStrafing(stats, ang, vel, buttons);
	}
	if (stats.wasOnGround && !onGround) { // Jumped
		ResetJump(stats);
	} else if (!stats.wasOnGround && onGround) { // Landed
		g_iStrafes[player] = stats.strafes;
		g_iSync[player] = stats.frames > 0 ? static_cast<int>(100.0 * stats.goodFrames / stats.frames) : 0;
		static cell cStrafes[MAX_STRAFES];
		for(int i = 0;i < stats.strafes;i++) {
			cStrafes[i] = static_cast<cell>(static_cast<int>(100.0 * stats.strafesGoodFrames[i] / stats.strafesFrames[i]));
		}
		cell cellStrafes = MF_PrepareCellArray(cStrafes, MAX_STRAFES);
        cell cellGain = amx_ftoc(stats.gain);
		//forward fwPlayerStrafe(id, strafes, sync, strafes[32], strafeLen, frames, goodFrames, Float:gain);
		MF_ExecuteForward(g_fwStrafe, player, stats.strafes, g_iSync[player], cellStrafes, stats.strafes, stats.frames, stats.goodFrames, cellGain, stats.overlaps);
	}

	stats.wasOnGround = onGround;
}