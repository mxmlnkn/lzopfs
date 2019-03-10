#include "FileList.h"

#include <iostream>

#include "LzopFile.h"
#include "PixzFile.h"
#include "GzipFile.h"
#include "Bzip2File.h"


const FileList::OpenerList FileList::Openers(initOpeners());

FileList::OpenerList FileList::initOpeners() {
	return { LzopFile::open, GzipFile::open, Bzip2File::open, PixzFile::open };
}


CompressedFile *FileList::find(const std::string& dest) {
	const auto found = mMap.find(dest);
	if (found == mMap.end())
		return nullptr;
	return found->second;
}

void FileList::add(const std::string& source) {
	CompressedFile *file = nullptr;
	try {
        /* try out opening the files with all available file opener functors */
		for (auto iter = Openers.begin(); iter != Openers.end(); ++iter) {
			try {
				file = (*iter)(source, mMaxBlockSize);
				break;
			} catch (CompressedFile::FormatException& e) {
				// just keep going
			}
		}
		if (!file) {
			std::cerr << "Don't understand format of file " << source.c_str() << ", skipping.\n";
			return;
		}

		const auto destPath = std::string("/") + file->destName();
        if ( mMap.find( destPath ) != mMap.end() ) {
            std::cerr
            << "The archive '" << source << "' was to be mounted at the FUSE mount point"
            << " at '" << destPath << "' but some other archives already is mounted"
            << " at this path. Won't mount " << source << "!\n";
            delete file;
            file = nullptr;
        } else  {
            mMap[destPath] = file;
        }
	} catch (std::runtime_error& e) {
		std::cerr << "Error reading file " << source.c_str() << ", skipping: " << e.what() << "\n";
	}
}

FileList::~FileList() {
    for ( auto& nameAndObject : mMap) {
		delete nameAndObject.second;
        nameAndObject.second = nullptr;
	}
}
