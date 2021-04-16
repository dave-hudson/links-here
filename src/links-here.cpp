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
    std::string rel_path_;              // Relative path to this page
    std::set<std::string> refs_;        // Refs from this page
    std::vector<std::string> inbound_;  // Inbound page links
};

/*
 * Scan a front matter line.  If we find a title then extract it.
 */
auto scan_front_matter_line(const std::string &s, page_and_refs &par) -> void {
    if (s.substr(0, 7) != "title: ") {
        return;
    }

    /*
     * Is the title double quoted?  If yes, we'll want to remove those.
     */
    auto s_size = s.size();
    size_t quote_offs = 0;
    if ((s[7] == '"') && (s[s_size - 1] == '"')) {
        quote_offs = 1;
    }

    par.title_ = s.substr(7 + quote_offs, s_size - 7 - (quote_offs * 2));
}

/*
 * Scan a markdown line.  If we find any ref or note shortcodes then record them.
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
        size_t strip_chars = 0;
        auto ref_index = hugo_template.find("ref ");
        if (ref_index != std::string::npos) {
            strip_chars = 4;
        } else {
            ref_index = hugo_template.find("note ");
            if (ref_index == std::string::npos) {
                break;
            }

            strip_chars = 5;
        }

        auto link = hugo_template.substr(strip_chars);
        link.erase(0, link.find_first_not_of(" \t"));
        link.erase(link.find_last_not_of(" \t") + 1);

        par.refs_.insert(link);

        start_index = close_index + 3;
    }
}

/*
 * Scan an index.md file.
 */
auto scan_index_md(const std::string &path_prefix, const std::string &rel_path, page_and_refs &par) -> void {
    auto path = path_prefix + '/' + rel_path;
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
auto scan_leaf_dirs(const std::string &path_prefix, const std::string &rel_path, std::map<std::string, page_and_refs> &par) -> void {
    auto path = path_prefix + '/' + rel_path;
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        std::cerr << "Unable to open dir: " << path << '\n';
        return;
    }

    dirent *de;
    while ((de = readdir(dir)) != NULL) {
        /*
         * Skip . and ..
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

        auto next_path = rel_path + '/';
        next_path.append(de->d_name);

        /*
         * Ignore anything that doesn't contain an index.md file.
         */
        auto rel_index_file = next_path + "/index.md";
        auto index_file = path_prefix + '/' + rel_index_file;
        struct stat st;
        if (stat(index_file.c_str(), &st)) {
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        page_and_refs new_par;
        auto link_name = std::string(de->d_name);
        new_par.link_ = link_name;
        new_par.rel_path_ = next_path;
        scan_index_md(path_prefix, rel_index_file, new_par);
        par.emplace(link_name, new_par);
    }

    closedir(dir);
}

/*
 * Scan directories.
 */
auto scan_dirs(const std::string &path_prefix, std::map<std::string, page_and_refs> &par) -> void {
    DIR *dir = opendir(path_prefix.c_str());
    if (!dir) {
        std::cerr << "Unable to open directory: " << path_prefix << '\n';
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

        std::string next_path(de->d_name);
        scan_leaf_dirs(path_prefix, next_path, par);
    }

    closedir(dir);
}

/*
 * Report the usage for this application.
 */
static auto usage(const char *name) -> void {
    std::cerr << "usage: " << name << " [OPTIONS]\n\n";
    std::cerr << "Options\n";
    std::cerr << "  -t     Test mode (don't write files)\n";
    std::cerr << "  -v     Verbose output (dump output file contents)\n\n";
}

/*
 * Entry point.
 */
auto main(int argc, char **argv) -> int {
    bool test_mode = false;
    bool verbose_mode = false;

    /*
     * parse the command line options
     */
    int ch;
    while ((ch = getopt(argc, argv, "tv?")) != -1) {
        switch (ch) {
        case 't':
            test_mode = true;
            break;

        case 'v':
            verbose_mode = true;
            break;

        case '?':
            usage(argv[0]);
            exit(-1);
        }
    }

    std::string path_prefix;

    if (optind < argc) {
        path_prefix = std::string(argv[optind]);
    } else {
        path_prefix = ".";
    }

    std::cout << argv[0] << ": scanning: " << path_prefix << '\n';

    /*
     * Find all the pages and the links they reference to.
     */
    std::map<std::string, page_and_refs> par;
    scan_dirs(path_prefix, par);

    /*
     * Scan the pages we found, check the links go somewhere and track where
     * inbound links come from.
     */
    bool success = true;
    for (const auto &i : par) {
        for (const auto &j: i.second.refs_) {
            auto it = par.find(j);
            if (it == par.end()) {
                std::cerr << "Page: " << i.first << "/index.md - ref '" << j << "' not found\n";
                success = false;
                continue;
            }

            it->second.inbound_.push_back(i.first);
        }
    }

    /*
     * If we didn't find files then we're done.
     */
    if (!success) {
        exit(-1);
    }

    /*
     * We now have all the pages and all their inbound links, so write out the links-here.md files.
     */
    for (const auto &i : par) {
        /*
         * We ignore anything that's not a "concepts" page.
         */
        if (i.second.rel_path_.compare(0, 9, "concepts/") != 0) {
            continue;
        }

        std::string links_here_text;
        std::string indexed_by_text;
        if (i.second.inbound_.size()) {
            for (const auto &j: i.second.inbound_) {
                /*
                 * If we've already got an explicit reference to a page then we don't need to create one here.
                 */
                if (i.second.refs_.find(j) != i.second.refs_.end()) {
                    continue;
                }

                auto it = par.find(j);
                if (it->second.rel_path_.compare(0, 8, "indexes/") == 0) {
                    indexed_by_text += "* [" + it->second.title_ + "](/" + it->second.rel_path_ + ")\n";
                } else {
                    links_here_text += "* [" + it->second.title_ + "](/" + it->second.rel_path_ + ")\n";
                }
            }
        }

        auto links_here_md_filename = path_prefix + '/' + i.second.rel_path_+ "/links-here.md";
        if (links_here_text.size()) {
            std::cout << "Generating: " << links_here_md_filename << '\n';

            if (verbose_mode) {
                std::cout << links_here_text << '\n';
            }
        }

        auto indexed_by_md_filename = path_prefix + '/' + i.second.rel_path_+ "/indexed-by.md";
        if (indexed_by_text.size()) {
            std::cout << "Generating: " << indexed_by_md_filename << '\n';

            if (verbose_mode) {
                std::cout << indexed_by_text << '\n';
            }
        }

        /*
         * If we're in test mode then don't write any files.
         */
        if (test_mode) {
            continue;
        }

        std::ofstream links_here_file(links_here_md_filename);
        if (!links_here_file.is_open()) {
            std::cerr << "Failed to open: " << links_here_md_filename << '\n';
            continue;
        }

        links_here_file << links_here_text;
        links_here_file.close();

        std::ofstream indexed_by_file(indexed_by_md_filename);
        if (!indexed_by_file.is_open()) {
            std::cerr << "Failed to open: " << indexed_by_md_filename << '\n';
            continue;
        }

        indexed_by_file << indexed_by_text;
        indexed_by_file.close();
    }

    return 0;
}

