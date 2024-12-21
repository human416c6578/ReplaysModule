#pragma once

#include <string>
#include <cstdint>
#include <bitset>
#include <vector>
#include <algorithm>
#include <stdexcept>

constexpr auto HEADER_TIMESTAMP_BYTE_SIZE = 8;
constexpr auto HEADER_VERSION_BYTE_SIZE = 2;
constexpr auto HEADER_MAP_BYTE_SIZE = 32;
constexpr auto HEADER_TIME_BYTE_SIZE = 24;
constexpr auto HEADER_NAME_BYTE_SIZE = 32;
constexpr auto HEADER_STEAMID_BYTE_SIZE = 24;
constexpr auto HEADER_INFO_BYTE_SIZE = 32;


constexpr auto FRAME_FLAGS_BYTE_SIZE = 3;

constexpr auto FRAME_TIMESTAMP_DELTA_BYTE_SIZE = 2;

constexpr auto ORIGIN_BYTE_SIZE = 2;
constexpr auto ORIGIN_BYTE_SIZE_DELTA = 1;

constexpr auto ANGLE_BYTE_SIZE = 2;
constexpr auto ANGLE_BYTE_SIZE_DELTA = 1;

constexpr auto SPEED_BYTE_SIZE = 2;
constexpr auto SPEED_BYTE_SIZE_DELTA = 1;

constexpr auto KEYS_BYTE_SIZE = 1;
constexpr auto FPS_BYTE_SIZE = 1;
constexpr auto STRAFES_BYTE_SIZE = 1;
constexpr auto SYNC_BYTE_SIZE = 1;


constexpr auto JUMP			= (1 << 0);
constexpr auto DUCK			= (1 << 1);
constexpr auto FORWARD		= (1 << 2);
constexpr auto BACK			= (1 << 3);
constexpr auto MOVELEFT		= (1 << 4);
constexpr auto MOVERIGHT		= (1 << 5);
constexpr auto LEFT			= (1 << 6);
constexpr auto RIGHT			= (1 << 7);

class FrameData
{
	int timestamp;
	int origin[3];
	int angles[2];
	int speed;
	int fps;
	int keys;
	bool grounded;
	bool gravity;
	int strafes;
	int sync;

public:
	FrameData(int timestamp, int origin[3], int angles[2], int speed, int fps, int keys, int strafes, int sync, bool grounded, bool gravity)
    : timestamp(timestamp), speed(speed), fps(fps), keys(keys), grounded(grounded), gravity(gravity), strafes(strafes), sync(sync)
	{
	    this->origin[0] = origin[0];
	    this->origin[1] = origin[1];
	    this->origin[2] = origin[2];
	
	    this->angles[0] = angles[0];
	    this->angles[1] = angles[1];
	}


	
	std::vector<uint8_t> encode();
	std::vector<uint8_t> encode_delta(FrameData prev_frame);
	int decode(const std::vector<uint8_t>& packed_data, FrameData* prev_frame = nullptr);

	int getTimestamp() const { return timestamp; }
    const int* getOrigin() const { return origin; } // Returns a pointer to the array
    const int* getAngles() const { return angles; } // Returns a pointer to the array
    int getSpeed() const { return speed; }
    int getFPS() const { return fps; }
    int getKeys() const { return keys; }
	int getStrafes() const { return strafes; }
	int getSync() const { return sync; }
    bool isGrounded() const { return grounded; }
    bool hasGravity() const { return gravity; }

	void print() const
	{
		printf("Timestamp: %d\n", timestamp);

		printf("Origin: [%d, %d, %d]\n", origin[0], origin[1], origin[2]);
		printf("Angles: [%d, %d]\n", angles[0], angles[1]);

		printf("Speed: %d\n", speed);
		printf("FPS: %d\n", fps);
		printf("Keys: %d\n", keys);

		printf("Grounded: %s\n", grounded ? "true" : "false");
		printf("Gravity: %s\n\n", gravity ? "true" : "false");

	}
	bool overlap() const
	{
		return (keys & MOVELEFT && keys & MOVERIGHT);
	}

	FrameData operator+(const FrameData& other) const {
		int new_timestamp = this->timestamp + other.timestamp;

		int new_origin[3];
		for (int i = 0; i < 3; i++) {
			new_origin[i] = this->origin[i] + other.origin[i];
		}

		int new_angles[2];
		for (int i = 0; i < 2; i++) {
			new_angles[i] = calc_angle_delta(this->angles[i], other.angles[i]);
		}

		int new_speed = this->speed + other.speed;
		int new_fps = this->fps;
		int new_keys = this->keys;
		int new_strafes = this->strafes;
		int new_sync = this->sync;

		bool new_grounded = this->grounded;
		bool new_gravity = this->gravity;

		return FrameData(new_timestamp, new_origin, new_angles, new_speed, new_fps, new_keys,new_strafes, new_sync, new_grounded, new_gravity);
	}

private:
	int convertKeys(int original_keys) {
		int compact_keys = 0;

		if (original_keys & (1 << 1)) compact_keys |= (1 << 0); // IN_JUMP -> compact bit 0
		if (original_keys & (1 << 2)) compact_keys |= (1 << 1); // IN_DUCK -> compact bit 1
		if (original_keys & (1 << 3)) compact_keys |= (1 << 2); // IN_FORWARD -> compact bit 2
		if (original_keys & (1 << 4)) compact_keys |= (1 << 3); // IN_BACK -> compact bit 3
		if (original_keys & (1 << 9)) compact_keys |= (1 << 4); // IN_MOVELEFT -> compact bit 4
		if (original_keys & (1 << 10)) compact_keys |= (1 << 5); // IN_MOVERIGHT -> compact bit 5
		if (original_keys & (1 << 7)) compact_keys |= (1 << 6); // IN_LEFT -> compact bit 6
		if (original_keys & (1 << 8)) compact_keys |= (1 << 7); // IN_RIGHT -> compact bit 7

		return compact_keys;
	}

	int decompactKeys(int compact_keys) {
    	int original_keys = 0;

    	if (compact_keys & (1 << 0)) original_keys |= (1 << 1);  // Compact bit 0 -> IN_JUMP
    	if (compact_keys & (1 << 1)) original_keys |= (1 << 2);  // Compact bit 1 -> IN_DUCK
    	if (compact_keys & (1 << 2)) original_keys |= (1 << 3);  // Compact bit 2 -> IN_FORWARD
    	if (compact_keys & (1 << 3)) original_keys |= (1 << 4);  // Compact bit 3 -> IN_BACK
    	if (compact_keys & (1 << 4)) original_keys |= (1 << 9);  // Compact bit 4 -> IN_MOVELEFT
    	if (compact_keys & (1 << 5)) original_keys |= (1 << 10); // Compact bit 5 -> IN_MOVERIGHT
    	if (compact_keys & (1 << 6)) original_keys |= (1 << 7);  // Compact bit 6 -> IN_LEFT
    	if (compact_keys & (1 << 7)) original_keys |= (1 << 8);  // Compact bit 7 -> IN_RIGHT

    	return original_keys;
	}



	static int clamp_angle(int angle)
	{
		if (angle > 900)
			angle -= 1800;
		else if (angle < -900)
			angle += 1800;

		return angle;
	}

	static int calc_angle_delta(int current_angle, int prev_angle)
	{
		int delta = current_angle - prev_angle;

		if (delta > 900)
			delta -= 1800;
		else if (delta < -900)
			delta += 1800;
			
		return delta;
	}
};
