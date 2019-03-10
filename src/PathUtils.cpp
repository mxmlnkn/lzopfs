#include "PathUtils.h"

#include <string>
#include <stdlib.h>

using std::string;


namespace PathUtils {

std::string basename(const std::string& path) {
	if (path.empty())
		return ".";

	size_t bend = path.find_last_not_of('/');
	if (bend == std::string::npos)
		return "/";

	size_t bstart = path.find_last_of('/', bend);
	return std::string(
		&path[bstart == std::string::npos ? 0 : bstart + 1],
		&path[bend + 1]
	);
}

size_t endsWith(const std::string& haystack, const std::string& needle) {
	if (haystack.size() < needle.size())
		return std::string::npos;
	if (haystack.compare(haystack.size() - needle.size(),
			needle.size(), needle) != 0)
		return std::string::npos;

	return haystack.size() - needle.size();
}

size_t hasExtension(const std::string& name, const std::string& ext) {
	std::string e(".");
	e.append(ext);

	size_t pos = endsWith(name, e);
	if (pos == 0 || pos == std::string::npos) // can't start with extension
		return std::string::npos;
	return pos + 1; // start of extension
}

bool replaceExtension(std::string& name, const std::string& ext, const std::string& replace) {
	size_t pos = hasExtension(name, ext);
	if (pos == std::string::npos)
		return false;
	name.replace(pos, name.size() - pos, replace);
	return true;
}

bool removeExtension(std::string& name, const std::string& ext) {
	size_t pos = hasExtension(name, ext);
	if (pos == std::string::npos)
		return false;
	name.resize(pos - 1);
	return true;
}

std::string realpath(const std::string& path) {
	char *abs = nullptr;
	try {
		abs = ::realpath(path.c_str(), NULL);
		string ret(abs);
		free(abs);
		return ret;
	} catch (...) {
		if (abs != nullptr)
            free(abs);
		throw;
	}
}

}
