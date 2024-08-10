#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;

const std::string SAVE_FILE_NAME = "DRAKS0005.sl2";
std::string TARGET_FILE;
std::string BACKUP_DIR;

std::string getFormattedTime(const std::chrono::system_clock::time_point& time, const std::string& format) {
    auto in_time_t = std::chrono::system_clock::to_time_t(time);
    std::tm tm_buf;
    localtime_s(&tm_buf, &in_time_t);
    std::stringstream ss;
    ss << std::put_time(&tm_buf, format.c_str());
    return ss.str();
}

bool safeFileCopy(const std::string& source, const std::string& destination) {
    std::ifstream src(source, std::ios::binary);
    if (!src) {
        std::cerr << "Failed to open source file: " << source << std::endl;
        return false;
    }
    std::ofstream dst(destination, std::ios::binary);
    if (!dst) {
        std::cerr << "Failed to create destination file: " << destination << std::endl;
        return false;
    }
    dst << src.rdbuf();
    return src && dst;
}

bool createBackupWithRetry(const std::string& suffix = "") {
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto now = std::chrono::system_clock::now();
        std::string dateDir = getFormattedTime(now, "%Y%m%d");
        std::string timeDir = getFormattedTime(now, "%H%M%S") + suffix;
        fs::path backupPath = fs::path(BACKUP_DIR) / dateDir / timeDir;
        fs::create_directories(backupPath);
        fs::path backupFilePath = backupPath / SAVE_FILE_NAME;
        try {
            if (safeFileCopy(TARGET_FILE, backupFilePath.string())) {
                std::cout << "Backup created: " << backupFilePath << std::endl;
                return true;
            } else {
                std::cerr << "Failed to create backup: " << backupFilePath << std::endl;
                if (attempt == 0) {
                    std::cout << "Retrying in 2 seconds..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during backup: " << e.what() << std::endl;
            if (attempt == 0) {
                std::cout << "Retrying in 2 seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }
    return false;
}

std::time_t getLastWriteTime(const std::string& filename) {
    try {
        return fs::last_write_time(filename).time_since_epoch().count();
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error getting last write time: " << e.what() << std::endl;
        return 0;
    }
}

void monitorAndBackup() {
    const int checkIntervalSec = 300;

    std::cout << "Monitoring file: " << TARGET_FILE << std::endl;
    std::cout << "Backup directory: " << BACKUP_DIR << std::endl;
    std::cout << "Checking for updates every " << checkIntervalSec << " seconds..." << std::endl;

    createBackupWithRetry("_start");
    auto lastWriteTime = getLastWriteTime(TARGET_FILE);

    while (true) {
        auto currentWriteTime = getLastWriteTime(TARGET_FILE);
        if (currentWriteTime != 0 && currentWriteTime != lastWriteTime) {
            createBackupWithRetry();
            lastWriteTime = currentWriteTime;
        }
        std::this_thread::sleep_for(std::chrono::seconds(checkIntervalSec));
    }
}

int main(int argc, char* argv[]) {
    std::cout << "DARK SOULS REMASTERED SAVE GUARDIAN is now running..." << std::endl;

    // Get user's Documents folder
    char* documentsPath = nullptr;
    size_t bufferSize;
    errno_t err = _dupenv_s(&documentsPath, &bufferSize, "USERPROFILE");
    if (err != 0 || documentsPath == nullptr) {
        std::cerr << "Unable to get user profile directory" << std::endl;
        return 1;
    }

    // Set TARGET_FILE
    TARGET_FILE = std::string(documentsPath) + "\\Documents\\FromSoftware\\DARK SOULS REMASTERED\\897047250\\" + SAVE_FILE_NAME;

    // Free the allocated memory
    free(documentsPath);

    // Check if backup directory is provided as command-line argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <backup_directory>" << std::endl;
        return 1;
    }
    BACKUP_DIR = argv[1];

    try {
        monitorAndBackup();
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
    }
    return 0;
}