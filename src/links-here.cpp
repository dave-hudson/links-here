/*
 * links-here.cpp
 */
#include <iostream>
#include <fstream>
#include <map>
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
    std::string title_;                 // Title of this page
    std::string link_;                  // Link name of this page
    std::string path_;                  // Path to this page
    std::set<std::string> refs_;        // Refs from this page
};

struct page_and_refs_cmp {
    bool operator()(const page_and_refs &lhs, const page_and_refs &rhs) const {
        return lhs.link_ < rhs.link_;
    }
};

/*
 * Scan a front matter line.
 */
auto scan_front_matter_line(const std::string &s, page_and_refs &par) -> void {
    if (s.substr(0, 7) != "title: ") {
        return;
    }

    par.title_ = s.substr(7);
}

/*
 * Scan a markdown line.
 */
auto scan_markdown_line(const std::string &s, page_and_refs &par) -> void {
    size_t start_index = 0;
    while (true) {
        auto open_index = s.find("{{<", start_index);
        if (open_index == std::string::npos) {
            break; }

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
    int phase = 0;
    while (getline(f, s)) {
        switch (phase) {
        case 0:                         // Find first ---
            if (s != "---") {
                break;
            }

            phase = 1;
            break;

        case 1:                         // Front matter
            if (s == "---") {
                phase = 2;
                break;
            }

            scan_front_matter_line(s, par);
            break;

        case 2:                         // Markdown
            scan_markdown_line(s, par);
            break;
        }
    }

    f.close();
}

/*
 * Scan leaf directories.
 */
auto scan_leaf_dirs(const std::string &path, std::set<page_and_refs, page_and_refs_cmp> &par) -> void {
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

        par.insert(new_par);
    }

    closedir(dir);
}

/*
 * Scan directories.
 */
auto scan_dirs(const std::string &path, std::set<page_and_refs, page_and_refs_cmp> &par) -> void {
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

    /*
     * Find all the pages and the links they reference to.
     */
    std::set<page_and_refs, page_and_refs_cmp> par;
    scan_dirs(path, par);

    /*
     * Scan the pages we found, check the links go somewhere and track where
     * inbound links come from.
     */
    std::map<std::string, std::vector<std::string>> inbound_links;
    for (const auto &i : par) {
        inbound_links[i.link_] = {};
    }

    bool success = true;
    for (const auto &i : par) {
        for (const auto &j: i.refs_) {
            auto it = inbound_links.find(j);
            if (it == inbound_links.end()) {
                std::cerr << "Page: " << i.path_ << "/index.md - ref '" << j << "' not found\n";
                success = false;
            } else {
                it->second.push_back(i.link_);
            }
        }
    }

    if (!success) {
        exit(-1);
    }

    for (const auto &i : inbound_links) {
        std::cout << "Page: " << i.first << ": ";
        for (const auto &j: i.second) {
            std::cout << j << ", ";
        }
        std::cout << '\n';
    }

    return 0;
}

