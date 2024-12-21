#include "amxxmodule.h"

#include "Replay.h"
#include <fstream>
#include <iostream>
#include <string>
#include "utils.h"

void Replay::encode(const std::string& output_filename)
{
    std::ofstream output_file(output_filename, std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "Error opening output file: " << output_filename << std::endl;
        return;
    }
    std::vector<uint8_t> encoded_data = encodeHeader(header);
    output_file.write(reinterpret_cast<const char*>(encoded_data.data()), encoded_data.size());

    if (frames.size() < 1)
    {
        std::cerr<<"No Frames in replay!";
        return;
    }
    encoded_data = frames[0].encode();
    output_file.write(reinterpret_cast<const char*>(encoded_data.data()), encoded_data.size());
    for (size_t i = 1; i < frames.size(); i++)
    {
        encoded_data = frames[i].encode_delta(frames[i - 1]);

        output_file.write(reinterpret_cast<const char*>(encoded_data.data()), encoded_data.size());
    }

}

Replay Replay::decode(const std::string& input_filename)
{
    Replay replay;

    std::ifstream input_file(input_filename, std::ios::binary);
    if (!input_file.is_open()) {
        std::cerr << "Error opening input file: " << input_filename << std::endl;
        return replay;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(input_file)),
        std::istreambuf_iterator<char>());
    input_file.close();

    size_t offset = 133;

    std::vector<uint8_t> header_data(buffer.begin(), buffer.begin() + offset);
    replay.header = Replay::decodeHeader(header_data);

    FrameData* prev_frame = nullptr;

    while (offset < buffer.size()) {
        // Create a vector for the current frame data starting from the current offset
        std::vector<uint8_t> frame_data(buffer.begin() + offset, buffer.end());

        // Decode the frame
        int origin[3] = { 0, 0, 0 };
        int angles[2] = { 0, 0 };
        FrameData current_frame(0, origin, angles, 0, 0, 0, 0, 0, false, false);

        // Update offset with the number of bytes processed in the current frame
        offset += current_frame.decode(frame_data, prev_frame);

        replay.addFrame(current_frame);

        // Update prev_frame to point to the current frame (for delta decoding)
        if (prev_frame != nullptr) {
            delete prev_frame;
        }
        prev_frame = new FrameData(current_frame);
    }

    // Clean up
    if (prev_frame != nullptr) {
        delete prev_frame;
    }

    return replay;
}

void Replay::addFrame(const FrameData frame)
{
    frames.push_back(frame);
}

std::vector<uint8_t> Replay::encodeHeader(const Header& header)
{
    std::vector<uint8_t> packed_header;

    // Encode timestamp (8 bytes)
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 56) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 48) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 40) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 32) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 24) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 16) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>((header.timestamp >> 8) & 0xFF));
    packed_header.push_back(static_cast<uint8_t>(header.timestamp & 0xFF));

    // Encode version (2 bytes)
    packed_header.push_back(static_cast<uint8_t>((header.version >> 8) & 0xFF)); // High byte
    packed_header.push_back(static_cast<uint8_t>(header.version & 0xFF));        // Low byte

    // Encode map name (32 bytes, padded or truncated)
    for (size_t i = 0; i < 32; ++i) {
        packed_header.push_back(i < header.map.size() ? header.map[i] : '\0');
    }

    // Encode time (3 bytes)
    packed_header.push_back(static_cast<uint8_t>((header.time >> 16) & 0xFF)); // MSB
    packed_header.push_back(static_cast<uint8_t>((header.time >> 8) & 0xFF));  // Middle byte
    packed_header.push_back(static_cast<uint8_t>(header.time & 0xFF));         // LSB

    // Encode player name (32 bytes, padded or truncated)
    for (size_t i = 0; i < 32; ++i) {
        packed_header.push_back(i < header.name.size() ? header.name[i] : '\0');
    }

    // Encode SteamID (24 bytes, padded or truncated)
    for (size_t i = 0; i < 24; ++i) {
        packed_header.push_back(i < header.steamID.size() ? header.steamID[i] : '\0');
    }

    // Encode additional info (32 bytes, padded or truncated)
    for (size_t i = 0; i < 32; ++i) {
        packed_header.push_back(i < header.info.size() ? header.info[i] : '\0');
    }

    return packed_header;
}

Header Replay::decodeHeader(const std::vector<uint8_t>& packed_header) {
    if (packed_header.size() < 133) {
        throw std::invalid_argument("Header data is incomplete or corrupted.");
    }

    Header header;
    size_t offset = 0;

    // Decode timestamp (8 bytes)
    header.timestamp =
        (static_cast<uint64_t>(packed_header[offset]) << 56) |  // Byte 1 (Most significant byte)
        (static_cast<uint64_t>(packed_header[offset + 1]) << 48) |  // Byte 2
        (static_cast<uint64_t>(packed_header[offset + 2]) << 40) |  // Byte 3
        (static_cast<uint64_t>(packed_header[offset + 3]) << 32) |  // Byte 4
        (static_cast<uint64_t>(packed_header[offset + 4]) << 24) |  // Byte 5
        (static_cast<uint64_t>(packed_header[offset + 5]) << 16) |  // Byte 6
        (static_cast<uint64_t>(packed_header[offset + 6]) << 8) |   // Byte 7
        static_cast<uint64_t>(packed_header[offset + 7]);           // Byte 8 (Least significant byte)
    offset += 8;  // Move to the next field after reading 8 bytes

    // Decode version (2 bytes)
    header.version = (static_cast<uint16_t>(packed_header[offset]) << 8) |
        static_cast<uint16_t>(packed_header[offset + 1]);

    offset += 2;

    // Decode map name (32 bytes, trim null padding)
    header.map = std::string(packed_header.begin() + offset, packed_header.begin() + offset + 32);
    header.map = header.map.substr(0, header.map.find('\0'));
    offset += 32;

    // Decode time (3 bytes)
    header.time = (static_cast<uint16_t>(packed_header[offset]) << 16) |
        (static_cast<uint16_t>(packed_header[offset + 1]) << 8) |
        static_cast<uint16_t>(packed_header[offset + 2]);

    offset += 3;

    // Decode player name (32 bytes, trim null padding)
    header.name = std::string(packed_header.begin() + offset, packed_header.begin() + offset + 32);
    header.name = header.name.substr(0, header.name.find('\0'));
    offset += 32;

    // Decode SteamID (24 bytes, trim null padding)
    header.steamID = std::string(packed_header.begin() + offset, packed_header.begin() + offset + 24);
    header.steamID = header.steamID.substr(0, header.steamID.find('\0'));
    offset += 24;

    // Decode additional info (32 bytes, trim null padding)
    header.info = std::string(packed_header.begin() + offset, packed_header.begin() + offset + 32);
    header.info = header.info.substr(0, header.info.find('\0'));
    offset += 32;

    return header;
}