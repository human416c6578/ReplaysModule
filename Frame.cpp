
#include "Frame.h"


std::vector<uint8_t> FrameData::encode()
{
    std::vector<uint8_t> packed_data;

    // Frame flags (3 bytes = 24 bits)
    uint32_t flags = 0;
    flags |= (1 << 0); // RLE
    flags |= (1 << 1); // ORIGIN X
    flags |= (1 << 2); // ORIGIN Y
    flags |= (1 << 3); // ORIGIN Z
    flags |= (1 << 4); // YAW ANGLE
    flags |= (1 << 5); // PITCH ANGLE
    flags |= (1 << 6); // SPEED
    flags |= (1 << 7); // KEYS presence
    flags |= (1 << 8); // FPS presence
    flags |= (1 << 9); // Strafes presence
    flags |= (1 << 10); // Sync presence
    flags |= (grounded ? (1 << 11) : 0); // Grounded status
    flags |= (gravity ? (1 << 12) : 0); // Gravity status

    // Write flags to packed data (3 bytes)
    packed_data.push_back((flags >> 16) & 0xFF);
    packed_data.push_back((flags >> 8) & 0xFF);
    packed_data.push_back(flags & 0xFF);

    // Encode timestamp delta (1 byte)
    packed_data.push_back(static_cast<uint8_t>(timestamp));

    // Encode origin values as 2 bytes per coordinate
    packed_data.push_back(static_cast<uint8_t>((origin[0] >> 8) & 0xFF)); // X high byte
    packed_data.push_back(static_cast<uint8_t>(origin[0] & 0xFF));        // X low byte
    packed_data.push_back(static_cast<uint8_t>((origin[1] >> 8) & 0xFF)); // Y high byte
    packed_data.push_back(static_cast<uint8_t>(origin[1] & 0xFF));        // Y low byte
    packed_data.push_back(static_cast<uint8_t>((origin[2] >> 8) & 0xFF)); // Z high byte
    packed_data.push_back(static_cast<uint8_t>(origin[2] & 0xFF));        // Z low byte

    // Encode angle values as 2 bytes per angle
    packed_data.push_back(static_cast<uint8_t>((angles[0] >> 8) & 0xFF)); // Yaw high byte
    packed_data.push_back(static_cast<uint8_t>(angles[0] & 0xFF));        // Yaw low byte
    packed_data.push_back(static_cast<uint8_t>((angles[1] >> 8) & 0xFF)); // Pitch high byte
    packed_data.push_back(static_cast<uint8_t>(angles[1] & 0xFF));        // Pitch low byte

    // Encode speed as 2 bytes
    packed_data.push_back(static_cast<uint8_t>((speed >> 8) & 0xFF)); // Speed high byte
    packed_data.push_back(static_cast<uint8_t>(speed & 0xFF));        // Speed low byte

    packed_data.push_back(static_cast<uint8_t>(convertKeys(keys)));

    packed_data.push_back(static_cast<uint8_t>(fps));

    packed_data.push_back(static_cast<uint8_t>(strafes));
    packed_data.push_back(static_cast<uint8_t>(sync));
    
    return packed_data;
}

std::vector<uint8_t> FrameData::encode_delta(FrameData prev_frame) {
    std::vector<uint8_t> packed_data;

    // Flags setup (3 bytes = 24 bits)
    uint32_t flags = 0;
    int flag_position = 1; // Start from bit 1 since bit 0 is reserved for RLE

    // Check if origin components are unchanged
    bool origin_unchanged = true;
    int origin_deltas[3];
    for (int i = 0; i < 3; ++i) {
        origin_deltas[i] = origin[i] - prev_frame.origin[i];
        if (origin_deltas[i] != 0) {
            origin_unchanged = false;
        }
    }

    // Check if angle components are unchanged
    bool angle_unchanged = true;
    int angle_deltas[2];
    for (int i = 0; i < 2; ++i) {
        angle_deltas[i] = calc_angle_delta(angles[i], prev_frame.angles[i]);
        if (angle_deltas[i] != 0) {
            angle_unchanged = false;
        }
    }

    // Set RLE flag (bit 0)
    bool rle_flag = origin_unchanged && angle_unchanged;
    flags |= (rle_flag ? 1 : 0);

    // If RLE is set, return only the flags, as no further data is needed
    if (rle_flag) {
        packed_data.push_back((flags >> 16) & 0xFF);
        packed_data.push_back((flags >> 8) & 0xFF);
        packed_data.push_back(flags & 0xFF);
        return packed_data;
    }

    // If RLE is not set, encode the rest of the fields
    bool origin_changed[3];
    for (int i = 0; i < 3; ++i) {
        origin_changed[i] = std::abs(origin_deltas[i]) > ((1 << ((ORIGIN_BYTE_SIZE_DELTA * 8) - 1)) - 1);
        flags |= (origin_changed[i] ? (1 << flag_position) : 0);  // Set flag bit for origin component
        flag_position++;
    }

    bool angle_changed[2];
    for (int i = 0; i < 2; ++i) {
        angle_changed[i] = std::abs(angle_deltas[i]) > ((1 << ((ANGLE_BYTE_SIZE_DELTA * 8) - 1)) - 1);
        flags |= (angle_changed[i] ? (1 << flag_position) : 0);  // Set flag bit for angle component
        flag_position++;
    }

    // Check if speed, keys, fps, and timestamp have changed
    bool speed_changed = std::abs(speed - prev_frame.speed) > ((1 << ((SPEED_BYTE_SIZE_DELTA * 8) - 1)) - 1);
    flags |= (speed_changed ? (1 << flag_position) : 0);
    flag_position++;

    bool keys_changed = keys != prev_frame.keys;
    flags |= (keys_changed ? (1 << flag_position) : 0);
    flag_position++;

    bool fps_changed = fps != prev_frame.fps;
    flags |= (fps_changed ? (1 << flag_position) : 0);
    flag_position++;

    bool strafes_changed = strafes != prev_frame.strafes;
    flags |= (strafes_changed ? (1 << flag_position) : 0);
    flag_position++;

    bool sync_changed = sync != prev_frame.sync;
    flags |= (sync_changed ? (1 << flag_position) : 0);
    flag_position++;

    flags |= (grounded ? (1 << flag_position) : 0);
    flag_position++;
    flags |= (gravity ? (1 << flag_position) : 0);
    flag_position++;

    // Write flags to packed data (3 bytes)
    packed_data.push_back((flags >> 16) & 0xFF);
    packed_data.push_back((flags >> 8) & 0xFF);
    packed_data.push_back(flags & 0xFF);

    // Encode timestamp delta (1 byte)
    int8_t delta_timestamp = static_cast<int8_t>(timestamp - prev_frame.timestamp);
    packed_data.push_back(static_cast<uint8_t>(delta_timestamp));

    // Encode origin components with full or delta size based on threshold
    for (int i = 0; i < 3; ++i) {
        if (origin_changed[i]) {
            packed_data.push_back(static_cast<uint8_t>((origin_deltas[i] >> 8) & 0xFF)); // High byte of full value
            packed_data.push_back(static_cast<uint8_t>(origin_deltas[i] & 0xFF));         // Low byte of full value
        }
        else {
            packed_data.push_back(static_cast<int8_t>(origin_deltas[i])); // Delta encoded in 1 byte
        }
    }

    // Encode angle components with full or delta size based on threshold
    for (int i = 0; i < 2; ++i) {
        if (angle_changed[i]) {
            packed_data.push_back(static_cast<uint8_t>((angle_deltas[i] >> 8) & 0xFF)); // High byte of full value
            packed_data.push_back(static_cast<uint8_t>(angle_deltas[i] & 0xFF));         // Low byte of full value
        }
        else {
            packed_data.push_back(static_cast<uint8_t>(angle_deltas[i])); // Delta encoded in 1 byte
        }
    }

    // Encode speed with full or delta size
    int speed_delta = speed - prev_frame.speed;
    if (speed_changed) {
        packed_data.push_back(static_cast<uint8_t>((speed_delta >> 8) & 0xFF)); // High byte of full value
        packed_data.push_back(static_cast<uint8_t>(speed_delta & 0xFF));        // Low byte of full value
    }
    else {
        packed_data.push_back(static_cast<uint8_t>(speed_delta)); // Delta encoded in 1 byte
    }

    // Encode keys if changed
    if (keys_changed) {
        packed_data.push_back(static_cast<uint8_t>(convertKeys(keys)));
    }

    // Encode fps if changed
    if (fps_changed) {
        packed_data.push_back(static_cast<uint8_t>(fps));
    }

    // Encode strafes if changed
    if (strafes_changed) {
        packed_data.push_back(static_cast<uint8_t>(strafes));
    }

    // Encode sync if changed
    if (sync_changed) {
        packed_data.push_back(static_cast<uint8_t>(sync));
    }

    return packed_data;
}

int FrameData::decode(const std::vector<uint8_t>& packed_data, FrameData* prev_frame) {
    size_t offset = 0;

    // Decode flags (3 bytes = 24 bits)
    uint32_t flags = (packed_data[offset] << 16) | (packed_data[offset + 1] << 8) | packed_data[offset + 2];
    offset += 3;

    // Check if this is the first frame (no previous frame available)
    if (prev_frame == nullptr) {

        // Decode full value for timestamp (assuming it's 1 byte for simplicity)
        timestamp = packed_data[offset++];

        // Decode full values for origin components (3 components, 2 bytes each)
        for (int i = 0; i < 3; ++i) {
            origin[i] = static_cast<int16_t>((packed_data[offset] << 8) | packed_data[offset + 1]);
            offset += 2;
        }

        // Decode full values for angle components (yaw and pitch, 2 bytes each)
        for (int i = 0; i < 2; ++i) {
            angles[i] = static_cast<int16_t>((packed_data[offset] << 8) | packed_data[offset + 1]);
            offset += 2;
        }

        // Decode full value for speed (2 bytes)
        speed = static_cast<int16_t>((packed_data[offset] << 8) | packed_data[offset + 1]);
        offset += 2;

        // Decode keys and fps as full values
        keys = static_cast<int16_t>(packed_data[offset++]);
        keys = decompactKeys(keys);
        fps = static_cast<int16_t>(packed_data[offset++]);

        strafes = static_cast<int16_t>(packed_data[offset++]);
        sync = static_cast<int16_t>(packed_data[offset++]);

        // Decode grounded and gravity flags directly from the flags
        grounded = flags & (1 << 11);
        gravity = flags & (1 << 12);

        return offset;
    }

    // Regular decoding process for subsequent frames with deltas
    bool rle_flag = flags & (1 << 0);
    if (rle_flag && prev_frame) {
        // If RLE is set, copy values from previous frame
        *this = *prev_frame;
        return offset;
    }

    int8_t delta = static_cast<int8_t>(packed_data[offset++]);
    timestamp = prev_frame->timestamp + delta;

    // Decode origin values based on flags
    for (int i = 0; i < 3; ++i) {
        bool full_value = flags & (1 << (i + 1));
        if (full_value) {
            // Full 2-byte value
            origin[i] = prev_frame->origin[i] + static_cast<int16_t>((packed_data[offset] << 8) | packed_data[offset + 1]);
            offset += 2;
        }
        else {
            // 1-byte delta
            int8_t delta = static_cast<int8_t>(packed_data[offset++]);
            origin[i] = prev_frame->origin[i] + delta;
        }
    }

    // Decode angle values based on flags
    for (int i = 0; i < 2; ++i) {
        bool full_value = flags & (1 << (i + 4));
        if (full_value) {
            // Full 2-byte value
            angles[i] = clamp_angle(prev_frame->angles[i] + static_cast<int16_t>((packed_data[offset] << 8) | packed_data[offset + 1]));
            offset += 2;
        }
        else {
            // 1-byte delta
            int8_t delta = static_cast<int8_t>(packed_data[offset++]);
            angles[i] = clamp_angle(prev_frame->angles[i] + delta);
        }
    }

    // Decode speed based on flags
    bool speed_full_value = flags & (1 << 6);
    if (speed_full_value) {
        // Full 2-byte value
        speed = prev_frame->speed + static_cast<int16_t>((packed_data[offset] << 8) | packed_data[offset + 1]);
        offset += 2;
    }
    else {
        // 1-byte delta
        int8_t delta = static_cast<int8_t>(packed_data[offset++]);
        speed = prev_frame->speed + delta;
    }

    // Decode keys if changed
    bool keys_changed = flags & (1 << 7);
    if (keys_changed) {
        keys = static_cast<int16_t>(packed_data[offset++]);
        keys = decompactKeys(keys);
    }
    else {
        keys = prev_frame->keys;
    }

    // Decode fps if changed
    bool fps_changed = flags & (1 << 8);
    if (fps_changed) {
        fps = static_cast<int16_t>(packed_data[offset++]);
    }
    else {
        fps = prev_frame->fps;
    }

    // Decode strafes if changed
    bool strafes_changed = flags & (1 << 9);
    if (strafes_changed) {
        strafes = static_cast<int16_t>(packed_data[offset++]);
    }
    else {
        strafes = prev_frame->strafes;
    }

    // Decode sync if changed
    bool sync_changed = flags & (1 << 10);
    if (sync_changed) {
        sync = static_cast<int16_t>(packed_data[offset++]);
    }
    else {
        sync = prev_frame->sync;
    }

    // Decode grounded and gravity flags
    grounded = flags & (1 << 11);
    gravity = flags & (1 << 12);
    
    return offset;
}

