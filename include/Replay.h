#pragma once
#include "Frame.h"

struct Header {
	uint64_t timestamp;       // 8 bytes
	uint16_t version;         // 2 bytes
	uint32_t time;			  // 3 bytes
	std::string map;          // 32 bytes
	std::string name;         // 32 bytes
	std::string steamID;      // 24 bytes
	std::string info;         // 32 bytes
};

class Replay {
	Header header;
	std::vector<FrameData> frames;

public:
	void encode(const std::string& output_filename);
	static Replay decode(const std::string& input_filename);
	
	static std::vector<uint8_t> encodeHeader(const Header& header);
	static Header decodeHeader(const std::vector<uint8_t>& packed_header);

	Header getHeader() { return header; }
	void setHeader(Header header) { this->header = header; }
	std::vector<FrameData>* getFrames() { return &frames; }


	void print() const
	{
		printf("\nTimestamp: %llu\nVersion: %u\nTime: %u\nMap: %s\nName: %s\nSteamID: %s\nINFO: %s\n\n",
			header.timestamp,
			header.version,
			header.time,
			header.map.c_str(),
			header.name.c_str(),
			header.steamID.c_str(),
			header.info.c_str());

	}
	void printFrames() const
	{
		for (size_t i = 0; i < frames.size(); i++)
			frames[i].print();
	}
	uint16_t overlap() const
	{
		uint16_t overlaps = 0;
		for (size_t i = 0; i < frames.size(); i++)
			if (frames[i].overlap())
				overlaps++;

		return overlaps;
	}

	void addFrame(const FrameData frame);
};