#include "amxxmodule.h"  // Include the AMX Mod X headers
#include "Replay.h"
#include "pm_defs.h"

#include <chrono>
#include <ctime>

#define DEBUG 0

#define REPLAY_FPS 60

const int targetIntervalMs = 1000 / REPLAY_FPS;

Replay g_Replays[33];
bool g_bRecording[33];
std::vector<Replay> g_BotReplays;
size_t g_iCurrentReplay = 0;
size_t g_iStrafes[33];
size_t g_iSync[33];

// TODO Header encoding/decoding or the way it's passed from pawn to module or viceverse is wrong
// I believe I have to do the smoothing again, use the timestamp for each frame to calculate how much the frame should stay on the screen

/*
enum eHeader{
	timestamp,
	version,
	time,
	map[64],
	name[64],
	steamID[32],
	info[32]
}

enum eFrame{
	timestamp,
	Float:origin[3],
	Float:angles[2],
	speed,
	fps,
	keys,
    strafes,
    sync,
	grounded,
	gravity
}
*/

float FastInvSqrt(float number) {
    float x2 = number * 0.5F;
    float y  = number;
    int i    = *reinterpret_cast<int*>(&y);      // Bit-level hack
    i        = 0x5f3759df - (i >> 1);            // Magic number
    y        = *reinterpret_cast<float*>(&i);
    y        = y * (1.5F - (x2 * y * y));        // First iteration
    // y      = y * (1.5F - (x2 * y * y));        // Optional second iteration
    return y;
}

// TODO: Add new thread for loading replays and add a callback function containing the header
// native LoadReplay(id, path[], header[eHeader]);
static cell AMX_NATIVE_CALL LoadReplay(AMX* amx, cell* params)
{   
    //int id = params[1]; // NOT USED

    int path_len;
    char buffer[128];
    char* path = MF_GetAmxString(amx, params[2], 0, &path_len);
    MF_BuildPathnameR(buffer, sizeof(buffer), "%s", path);

    // Decode replay file
    Replay replay = Replay::decode(std::string(buffer, sizeof(buffer)));
    Header header = replay.getHeader();
    
#if DEBUG
    printf("[DEBUG] Loading: \n");
    replay.print();
#endif
    cell* cpHeader = MF_GetAmxAddr(amx, params[3]);

    // Copy scalar values
    cpHeader[0] = static_cast<cell>(header.timestamp); // timestamp
    cpHeader[1] = static_cast<cell>(header.version);   // version
    cpHeader[2] = static_cast<cell>(header.time);      // time
    
    // Copy map string (index 3, size 64)
    for (size_t i = 0; i < 64 && i < header.map.length(); ++i) {
        cpHeader[3 + i] = static_cast<cell>(header.map[i]);
    }
    cpHeader[3 + header.map.length()] = '\0'; // Null-terminate
    
    // Copy name string (index 67, size 64)
    for (size_t i = 0; i < 64 && i < header.name.length(); ++i) {
        cpHeader[67 + i] = static_cast<cell>(header.name[i]);
    }
    cpHeader[67 + header.name.length()] = '\0'; // Null-terminate
    
    // Copy steamID string (index 131, size 32)
    for (size_t i = 0; i < 32 && i < header.steamID.length(); ++i) {
        cpHeader[131 + i] = static_cast<cell>(header.steamID[i]);
    }
    cpHeader[131 + header.steamID.length()] = '\0'; // Null-terminate
    
    // Copy info string (index 163, size 32)
    for (size_t i = 0; i < 32 && i < header.info.length(); ++i) {
        cpHeader[163 + i] = static_cast<cell>(header.info[i]);
    }
    cpHeader[163 + header.info.length()] = '\0'; // Null-terminate

    g_BotReplays.push_back(replay);

    g_iCurrentReplay = g_BotReplays.size() - 1;

    return 1;
}

void CalculateStrafes(struct playermove_s *pMove, int player) {
    static int old_buttons[33], old_onground[33];
    static int strafes[33];
    static int good_sync[33];
    static int frames[33];
    static vec3_t old_ang[33];
    static int old_speed[33];
    vec3_t ang = pMove->angles;
    vec3_t vel = pMove->velocity;
    int buttons = pMove->oldbuttons;
    int speed = static_cast<int>(vel.x * vel.x + vel.y * vel.y);
    int turning = false;

    // In Air
    if(!pMove->onground){
        if(ang != old_ang[player])
	    	turning = true;

        if(turning) {
            // Check for strafe transitions
            if (((old_buttons[player] & IN_MOVELEFT) && (buttons & IN_MOVERIGHT)) || ((old_buttons[player] & IN_MOVERIGHT) && (buttons & IN_MOVELEFT))) {
                strafes[player]++;
            } else if (((old_buttons[player] & IN_BACK) && (buttons & IN_FORWARD)) || ((old_buttons[player] & IN_FORWARD) && (buttons & IN_BACK))) {
                strafes[player]++;
            }

            if(speed > old_speed[player])
		    	good_sync[player]++;
    
		    frames[player]++;
        }
    }

    // Jumped
    if(old_onground[player] && !pMove->onground) {
        frames[player] = 0;
        good_sync[player] = 0;
        strafes[player] = 0;
    }
    // Landed
    else if(!old_onground[player] && pMove->onground) {
        g_iSync[player] = static_cast<int>(100.0 * good_sync[player] / frames[player]);
        g_iStrafes[player] = strafes[player];
    }

    old_speed[player] = speed;
    old_buttons[player] = buttons;
    old_onground[player] = pMove->onground;
    old_ang[player] = ang;
}

//TODO: Implement functionability to not record a player's movement when he's not moving at all or stop the recording entirely
void PM_Move(struct playermove_s *pMove, qboolean server) {
    if (pMove->dead)
        return;

    int player = pMove->player_index + 1;

    if (!g_bRecording[player])
        return;

    //CalculateStrafes(pMove, player);

    // Fetch position, velocity, and angles
    vec3_t vel = pMove->velocity;
    vec3_t ang = pMove->angles;
    vec3_t org = pMove->origin;

    // Static variables for up to 33 players
    static uint32_t timeSinceLastExecution[33] = { 0 };
    static uint32_t timeSinceLastFpsCount[33] = { 0 };
    static int fps[33] = { 0 };
    static int fpsCounter[33] = { 0 };

    uint32_t frametimeMs = static_cast<uint32_t>(pMove->frametime * 1000);
    uint32_t& playerExecutionTime = timeSinceLastExecution[player];

    fpsCounter[player]++;

    if (timeSinceLastFpsCount[player] >= 1000) {
        fps[player] = fpsCounter[player];
        fpsCounter[player] = 0;
        timeSinceLastFpsCount [player]= 0;
    }

    timeSinceLastFpsCount[player] += frametimeMs;

    if (playerExecutionTime < targetIntervalMs) {
        playerExecutionTime += frametimeMs;
        return;
    }

    // Convert origin and angles (scaled)
    int origin[3] = { static_cast<int>(org.x * 4), static_cast<int>(org.y * 4), static_cast<int>(org.z * 4) };
    int angles[2] = { static_cast<int>(ang.x * 5), static_cast<int>(ang.y * 5) };

    // Calculate speed (magnitude of velocity in the horizontal plane)
    int speed = static_cast<int>(1.0f / FastInvSqrt(vel.x * vel.x + vel.y * vel.y));

    // Track old button states
    int old_buttons = pMove->oldbuttons;

    // Create FrameData with elapsed time as the timestamp
    FrameData frame(playerExecutionTime, origin, angles, speed, fps[player] / 4, old_buttons, g_iStrafes[player], g_iSync[player], (pMove->onground == 0), (pMove->gravity == 1.0f));

    // Add the frame to the replay data for the player
    g_Replays[player].addFrame(frame);

    playerExecutionTime = 0;
}

// native SaveReplay(path[], id, map, authid, category, time);
static cell AMX_NATIVE_CALL SaveReplay(AMX* amx, cell* params)
{
    // Get player ID
    int id = params[2];

    // Ensure ID is within bounds of the g_Replays array
    if (id < 0 || id >= 33) return 0;  // Just return 0 if the ID is invalid
    
    g_bRecording[id] = false;

    // Fetch map name and category name (make sure these are correct)
    int pathLen, mapLen, authidLen, categoryLen;
    char* path = MF_GetAmxString(amx, params[1], 0, &pathLen);
    char* map = MF_GetAmxString(amx, params[3], 1, &mapLen);
    char* authid = MF_GetAmxString(amx, params[4], 2, &authidLen);
    char* category = MF_GetAmxString(amx, params[5], 3, &categoryLen);
    int time = params[6];

    // Setup the replay header
    Header header;
    header.version = 100;
    header.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    header.map = std::string(map, mapLen);
    header.info = std::string(category, categoryLen);
    header.time = time;
    header.name = MF_GetPlayerName(id);
    header.steamID = std::string(authid, authidLen);

    // Set the header for this replay
    g_Replays[id].setHeader(header);

#if DEBUG
    printf("[DEBUG] Saving: \n");
    replay.print();
#endif
    // Save the replay to a file (you can specify the file path you want)
    char buffer[128];
    MF_BuildPathnameR(buffer, sizeof(buffer), "%s", std::string(path, pathLen).c_str());

    g_Replays[id].encode(std::string(buffer, 127));

    g_Replays[id].getFrames()->clear();
    
    return 1;
}

// native StartRecord(id);
static cell AMX_NATIVE_CALL StartRecord(AMX* amx, cell* params)
{
    // Get player ID
    int id = params[1];
    g_Replays[id].getFrames()->clear();
    g_bRecording[id] = true;

    return 1;
}

// native StopRecord(id);
static cell AMX_NATIVE_CALL StopRecord(AMX* amx, cell* params)
{
    // Get player ID
    int id = params[1];
    g_bRecording[id] = false;

    return 1;
}


// native GetFrame(i, frame[eFrame]);
static cell AMX_NATIVE_CALL GetFrame(AMX* amx, cell* params)
{
    // Get the pointer to the frame array
    int frameId = params[1];
    cell* cpFrame = MF_GetAmxAddr(amx, params[2]);

    if(g_BotReplays.empty())
        return 0;

    if (g_iCurrentReplay >= g_BotReplays.size()) {
        g_iCurrentReplay = 0;
    }

    // Get the current replay
    Replay& currentReplay = g_BotReplays.at(g_iCurrentReplay);
    auto frames = currentReplay.getFrames(); // Cache reference to frames vector

    if(frames->empty())
        return 0;

    // Get the next frame
    const FrameData& frame = frames->at(frameId);
#if DEBUG
    frame.print();
#endif
    const int* originPtr = frame.getOrigin();
    const int* anglesPtr = frame.getAngles();

    float origin[3];
    origin[0] = (float)originPtr[0] / 4.0;
    origin[1] = (float)originPtr[1] / 4.0;
    origin[2] = (float)originPtr[2] / 4.0;

    float angles[3];
    angles[0] = (float)anglesPtr[0] / 5.0;
    angles[1] = (float)anglesPtr[1] / 5.0;
    angles[2] = (float)anglesPtr[2] / 5.0;

    // Copy scalar values into the frame array
    cpFrame[0] = static_cast<cell>(frame.getTimestamp());
    cpFrame[1] = amx_ftoc(origin[0]);
    cpFrame[2] = amx_ftoc(origin[1]);
    cpFrame[3] = amx_ftoc(origin[2]);
    cpFrame[4] = amx_ftoc(angles[0]);
    cpFrame[5] = amx_ftoc(angles[1]);
    cpFrame[6] = static_cast<cell>(frame.getSpeed());
    cpFrame[7] = static_cast<cell>(frame.getFPS() * 4); // has to be multiplied by 4
    cpFrame[8] = static_cast<cell>(frame.getKeys());
    cpFrame[9] = static_cast<cell>(frame.getStrafes());
    cpFrame[10] = static_cast<cell>(frame.getSync());
    cpFrame[11] = static_cast<cell>(frame.isGrounded());
    cpFrame[12] = static_cast<cell>(frame.hasGravity());

    return 1;
}

// native GetCurrentReplay();
static cell AMX_NATIVE_CALL GetCurrentReplay(AMX* amx, cell* params)
{
    return g_iCurrentReplay;
}

// native SetCurrentReplay();
static cell AMX_NATIVE_CALL SetCurrentReplay(AMX* amx, cell* params)
{
    if (g_BotReplays.empty())
        return 0;

    g_iCurrentReplay = params[1];
    if (g_iCurrentReplay >= g_BotReplays.size()) {
        g_iCurrentReplay = 0;  // Loop back to the first replay
    }


    return 1;
}

// native NextReplay();
static cell AMX_NATIVE_CALL NextReplay(AMX* amx, cell* params)
{
    if (g_BotReplays.empty())
        return 0;

    g_iCurrentReplay++;
    if (g_iCurrentReplay >= g_BotReplays.size()) {
        g_iCurrentReplay = 0;  // Loop back to the first replay
    }


    return 1;
}

// native DeleteReplay(replayId);
static cell AMX_NATIVE_CALL DeleteReplay(AMX* amx, cell* params)
{
    if (g_BotReplays.empty())
        return 0;

    int replayId = params[1];

    // Delete the current replay
    g_BotReplays.erase(g_BotReplays.begin() + replayId);

    g_iCurrentReplay = g_BotReplays.size() - 1;

    return 1;
}

// native GetReplaySize();
static cell AMX_NATIVE_CALL GetReplaySize(AMX* amx, cell* params)
{
    if (g_BotReplays.empty())
        return 0;

    return g_BotReplays.at(g_iCurrentReplay).getFrames()->size();
}

// Array of native functions to register with AMX Mod X
AMX_NATIVE_INFO my_natives[] = {
    { "LoadReplay", LoadReplay },
    { "SaveReplay", SaveReplay },
    { "StartRecord", StartRecord },
    { "StopRecord", StopRecord },
    { "GetCurrentReplay", GetCurrentReplay },
    { "SetCurrentReplay", SetCurrentReplay },
    { "GetFrame", GetFrame },
    { "NextReplay", NextReplay },
    { "DeleteReplay", DeleteReplay },
    { "GetReplaySize", GetReplaySize},
    { nullptr, nullptr }  // Array terminator
};

void OnAmxxAttach()
{
    MF_AddNatives(my_natives);
    // Add the natives from "modtuto.h".
}

void OnAmxxDetach()
{
    // This function is necessary, even if you have nothing to declare here. The compiler will throw a linker error otherwise.
    // This can be useful for clearing/destroying a handles system.
}

// Changelevel
void ServerDeactivate()
{
    for(int i=0;i<33;i++)
    {
        g_Replays[i].getFrames()->clear();
        g_bRecording[i] = false;
    }
    g_BotReplays.clear();
}