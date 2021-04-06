/*
 * links-here.cpp
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Collect ref codes.
 */
auto collect_ref_codes(const std::string &path) -> void {
    std::fstream f;
    f.open(path, std::ios::in);
    if (!f.is_open()) {
        std::cerr << "Cannot open '" << path << "'\n";
        return;
    }
std::cerr << "open '" << path << "'\n";

    std::string s;
    while (getline(f, s)) {
        std::cout << s << '\n';
    }

    f.close();
}

/*
 * Scan leaf directories.
 */
auto scan_leaf_dirs(const std::string &path, std::vector<std::string> &res) -> void {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return;
    }

    dirent *de;
    while ((de = readdir(dir)) != NULL) {
        /*
         * Skip "." and "..".
         */
        if (!strcmp(de->d_name, ".")) {
            continue;
        }

        if (!strcmp(de->d_name, "..")) {
            continue;
        }

        /*
         * Skip files.
         */
        if (!(de->d_type & DT_DIR)) {
            continue;
        }

        auto next_path = path + '/';
        next_path.append(de->d_name);

        /*
         * Ignore anything that doesn't contain an index.md file.
         */
        auto index_file = next_path + "/index.md";
        struct stat st;
        if (stat(index_file.c_str(), &st)) {
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        collect_ref_codes(index_file);

        std::cout << next_path << '\n';
        res.push_back(next_path);
    }

    closedir(dir);
}

/*
 * Scan directories.
 */
auto scan_dirs(const std::string &path, std::vector<std::string> &res) -> void {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return;
    }

    dirent *de;
    while ((de = readdir(dir)) != NULL) {
        /*
         * Skip "." and "..".
         */
        if (!strcmp(de->d_name, ".")) {
            continue;
        }

        if (!strcmp(de->d_name, "..")) {
            continue;
        }

        /*
         * Skip files.
         */
        if (!(de->d_type & DT_DIR)) {
            continue;
        }

        auto next_path = path + '/';
        next_path.append(de->d_name);
        scan_leaf_dirs(next_path, res);   
    }

    closedir(dir);
}

/*
 * Entry point.
 */
auto main(int argc, char **argv) -> int {
    std::vector<std::string> dirs;
    scan_dirs("content", dirs);
    return 0;
}

