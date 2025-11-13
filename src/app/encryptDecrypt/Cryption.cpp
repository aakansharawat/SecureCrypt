// Cryption.cpp

#include "Cryption.hpp"
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <string>
#include <fstream> 
#include <filesystem>

#include "../fileHandling/IO.hpp" 
#include "../fileHandling/ReadEnv.hpp" 

using namespace std;
namespace fs = std::filesystem;

// Function to handle encryption/decryption tasks called by a worker process.
int processCryptionTask(const string& taskData) {

    // Parse ACTION,FILEPATH,KEY (KEY optional)
    size_t p1 = taskData.find(',');
    if (p1 == string::npos) {
        if (taskData.find("STOP_WORKER_SIGNAL") == 0) {
            return 0;
        }
        cerr << "Invalid task data format. Expected 'ACTION,FILEPATH[,KEY]'." << endl;
        return 1;
    }
    size_t p2 = taskData.find(',', p1 + 1);
    string action = taskData.substr(0, p1);
    string filePath;
    string secretKey;
    if (p2 == string::npos) {
        filePath = taskData.substr(p1 + 1);
        secretKey = "";
    } else {
        filePath = taskData.substr(p1 + 1, p2 - (p1 + 1));
        secretKey = taskData.substr(p2 + 1);
    }
    // Handle stop signal
    if (action == "STOP_WORKER_SIGNAL") {
        return 0;
    }
    
    ReadEnv env;
    string envKey = ReadEnv::getenv();
    int envKeyInt;
    try {
        envKeyInt = stoi(envKey);
    } catch (const exception& e) {
        cerr << "Error converting ENV key to integer: " << e.what() << endl;
        return 1;
    }

    auto fnv1a32 = [](const string& k) -> uint32_t {
        const uint32_t FNV_OFFSET = 2166136261u;
        const uint32_t FNV_PRIME  = 16777619u;
        uint32_t hash = FNV_OFFSET;
        for (unsigned char c : k) {
            hash ^= c;
            hash *= FNV_PRIME;
        }
        return hash;
    };
    auto legacy32 = [](const string& k) -> uint32_t {
        size_t h = std::hash<string>{}(k);
        return static_cast<uint32_t>(h & 0xFFFFFFFFu);
    };
    auto deriveKeyIntFNV = [&](const string& k) -> int {
        uint32_t h = fnv1a32(k);
        return static_cast<int>(h % 256);
    };
    auto deriveKeyTagFNV = [&](const string& k) -> uint32_t {
        return fnv1a32(k);
    };
    auto deriveKeyIntLegacy = [&](const string& k) -> int {
        uint32_t h = legacy32(k);
        return static_cast<int>(h % 256);
    };
    auto deriveKeyTagLegacy = [&](const string& k) -> uint32_t {
        return legacy32(k);
    };

    // Decide key based on action and presence of sidecar lock
    int key = envKeyInt;
    fs::path inputPath = fs::path(filePath);
    bool hasPassword = !secretKey.empty();
    fs::path sidecarPath = inputPath.string() + ".lock";
    // Legacy mode: if secretKey begins with "legacy::", use legacy derivation
    bool legacyModeRequested = false;
    if (!secretKey.empty() && secretKey.rfind("legacy::", 0) == 0) {
        legacyModeRequested = true;
        secretKey = secretKey.substr(sizeof("legacy::") - 1);
    }

    if (action == "encrypt") {
        if (!secretKey.empty()) {
            key = legacyModeRequested ? deriveKeyIntLegacy(secretKey) : deriveKeyIntFNV(secretKey);
        }
    } else { // decrypt
        bool hasLock = fs::exists(sidecarPath);
        if (!secretKey.empty()) {
            // Caller provided a password: use it regardless of lock presence
            if (hasLock) {
                // Verify key hash if lock exists
                ifstream metaIn(sidecarPath);
                if (!metaIn.is_open()) {
                    cerr << "Failed to open lock file for: " << filePath << endl;
                    return 1;
                }
                uint32_t storedTag = 0;
                metaIn >> storedTag;
                metaIn.close();
                uint32_t providedTagF = deriveKeyTagFNV(secretKey);
                uint32_t providedTagL = deriveKeyTagLegacy(secretKey);
                if (storedTag == providedTagF) {
                    legacyModeRequested = false;
                } else if (storedTag == providedTagL) {
                    legacyModeRequested = true;
                } else {
                    cerr << "Invalid secret key for: " << filePath << endl;
                    return 1;
                }
            }
            key = legacyModeRequested ? deriveKeyIntLegacy(secretKey) : deriveKeyIntFNV(secretKey);
        } else {
            // No password provided
            if (hasLock) {
                cerr << "Decryption requires secret key for: " << filePath << endl;
                return 1;
            } else {
                // No lock: use ENV key
                key = envKeyInt;
            }
        }
    }

    {
        IO io(filePath); 
        fstream& f_stream = io.getFileStream(); 

        if (!f_stream.is_open()) {
            cerr << "Worker failed to open file: " << filePath << endl;
            return 1;
        }

        f_stream.seekg(0, ios::beg);
        f_stream.seekp(0, ios::beg);

        char ch;
        while (f_stream.get(ch)) {
            unsigned char uch = static_cast<unsigned char>(ch);
            streampos currentPos = f_stream.tellg(); 
            if (action == "encrypt") {
                uch = static_cast<unsigned char>((uch + key) % 256);
            } else { 
                uch = static_cast<unsigned char>((uch - key + 256) % 256);
            }
            f_stream.seekp(currentPos - static_cast<streamoff>(1));
            f_stream.put(static_cast<char>(uch));
            f_stream.flush();

            f_stream.seekg(currentPos);
        }
    }

    // Post-processing: rename and sidecar policy
    if (action == "encrypt") {
        if (hasPassword) {
            // Rename to .encrypted and write sidecar next to the renamed file
            fs::path encryptedPath = inputPath;
            encryptedPath += ".encrypted";
            std::error_code ec;
            fs::rename(inputPath, encryptedPath, ec);
            if (ec) {
                cerr << "Warning: could not rename to .encrypted for: " << filePath << " error: " << ec.message() << endl;
            }
            fs::path encSidecar = (ec ? sidecarPath : fs::path(encryptedPath.string() + ".lock"));
            ofstream metaOut(encSidecar);
            if (metaOut.is_open()) {
                if (legacyModeRequested) metaOut << deriveKeyTagLegacy(secretKey); else metaOut << deriveKeyTagFNV(secretKey);
                metaOut.close();
            } else {
                cerr << "Warning: could not write lock file for: " << (ec ? inputPath : encryptedPath) << endl;
            }
        } else {
            // No password: make sure no lock remains
            if (fs::exists(sidecarPath)) {
                std::error_code ec; fs::remove(sidecarPath, ec);
            }
        }
    } else { // decrypt
        if (hasPassword) {
            // If file ends with .encrypted, drop it after successful decrypt
            std::string fname = inputPath.filename().string();
            bool endsWithEncrypted = fname.size() >= 10 && fname.rfind(".encrypted") == fname.size() - 10;
            if (endsWithEncrypted) {
                fs::path targetPath = inputPath;
                targetPath.replace_filename(fname.substr(0, fname.size() - 10));
                std::error_code ec;
                fs::rename(inputPath, targetPath, ec);
                if (ec) {
                    cerr << "Warning: could not rename decrypted file to original name for: " << filePath << " error: " << ec.message() << endl;
                } else {
                    // Remove sidecars if present
                    std::error_code ec1; fs::remove((inputPath.string() + ".lock"), ec1);
                    std::error_code ec2; fs::remove((targetPath.string() + ".lock"), ec2);
                }
            } else {
                // Remove any sidecar that matches current name
                std::error_code ec; fs::remove(sidecarPath, ec);
            }
        } else {
            // ENV key mode: ensure no lock remains
            if (fs::exists(sidecarPath)) { std::error_code ec; fs::remove(sidecarPath, ec); }
        }
    }

    time_t t = time(nullptr);
    tm* now = localtime(&t);
    cout << "Worker completed " << action << " on " << filePath << " at: " 
             << put_time(now, "%Y-%m-%d %H:%M:%S") << endl;
    
    return 0;
}