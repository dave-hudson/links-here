/*
 * links-here.cpp
 */
#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <string>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Track page refs and their paths.
 */
struct page_and_refs {
    std::string link_;                  // Link name
    std::string path_;                  // Path to link
    std::set<std::string> refs_;        // Refs from this page
};

/*
 * Scan a line.
 */
auto scan_line(const std::string &s, page_and_refs &par) -> void {
    size_t start_index = 0;
    while (true) {
        auto open_index = s.find("{{<", start_index);
        if (open_index == std::string::npos) {
            break;
        }

        auto close_index = s.find(">}}", open_index + 3);
        if (close_index == std::string::npos) {
            break;
        }

        auto hugo_template = s.substr(open_index + 3, close_index - open_index - 3);
        auto ref_index = hugo_template.find("ref ");
        if (ref_index == std::string::npos) {
            break;
        }

        auto link = hugo_template.substr(4);
        link.erase(0, link.find_first_not_of(" \t"));
        link.erase(link.find_last_not_of(" \t") + 1);

        par.refs_.insert(link);

        start_index = close_index + 3;
    }
}

/*
 * Collect ref codes.
 */
auto collect_ref_codes(const std::string &path, page_and_refs &par) -> void {
    std::fstream f;
    f.open(path, std::ios::in);
    if (!f.is_open()) {
        std::cerr << "Cannot open '" << path << "'\n";
        return;
    }
    std::string s;
    while (getline(f, s)) {
        scan_line(s, par);
    }

    f.close();
}

/*
 * Scan leaf directories.
 */
auto scan_leaf_dirs(const std::string &path, std::vector<page_and_refs> &par) -> void {
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

        page_and_refs new_par;
        new_par.link_ = std::string(de->d_name);
        new_par.path_ = next_path;
        collect_ref_codes(index_file, new_par);

        par.push_back(new_par);
    }

    closedir(dir);
}

/*
 * Scan directories.
 */
auto scan_dirs(const std::string &path, std::vector<page_and_refs> &par) -> void {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        std::cerr << "Unable to open directory: " << path << '\n';
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
        scan_leaf_dirs(next_path, par);
    }

    closedir(dir);
}

/*
 * Entry point.
 */
auto main(int argc, char **argv) -> int {
    std::string path;

    if (argc == 2) {
        path = std::string(argv[1]);
    } else {
        path = ".";
    }

    std::vector<page_and_refs> par;
    scan_dirs(path, par);

    for (const auto &i : par) {
        std::cout << i.link_ << " at " << i.path_;
        for (const auto &j: i.refs_) {
            std::cout << ' ' << j;
        }
        std::cout << '\n';
    }

    return 0;
}

