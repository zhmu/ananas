#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace util {

// Splits 's' based on splitOnFn provided separators
template<typename Fn>
std::vector<std::string> Split(const std::string& s, Fn splitOnFn)
{
	std::vector<std::string> result;

	for (auto pos = s.begin(); pos != s.end(); /* nothing */) {
		// Look for the first s
		auto separator = std::find_if(pos, s.end(), splitOnFn);

		// Isolate piece [ pos .. separator ]
		result.push_back(std::string(pos, separator));

		// And move on to the next piece
		pos = std::find_if(separator, s.end(), [&](char ch) {
			return !splitOnFn(ch);
		});
	}
	return result;
}

// Splits 's' on spaces
inline std::vector<std::string> Split(const std::string& s)
{
	return Split(s, [](char ch) {
		return std::isspace(ch);
	});
}


} // namespace util

