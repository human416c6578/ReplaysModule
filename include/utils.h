#pragma once

#include <string>
#include <stdexcept>

class utils
{
	public:
		static int convertToMilliseconds(const std::string& timeString) {
			// Ensure the input string ends with 's'
			if (timeString.back() != 's') {
				throw std::invalid_argument("Invalid time format. Must end with 's'.");
			}

			// Remove the trailing 's'
			std::string cleanString = timeString.substr(0, timeString.size() - 1);

			// Find the position of the colon and the dot
			size_t colonPos = cleanString.find(':');
			size_t dotPos = cleanString.find('.');

			if (colonPos == std::string::npos || dotPos == std::string::npos || colonPos >= dotPos) {
				throw std::invalid_argument("Invalid time format. Expected 'M:SS.mmm'.");
			}

			// Extract minutes, seconds, and milliseconds
			int minutes = std::stoi(cleanString.substr(0, colonPos));
			int seconds = std::stoi(cleanString.substr(colonPos + 1, dotPos - colonPos - 1));
			int milliseconds = std::stoi(cleanString.substr(dotPos + 1));

			// Calculate total milliseconds
			int totalMilliseconds = (minutes * 60 * 1000) + (seconds * 1000) + milliseconds;
			return totalMilliseconds;
		}
		
};