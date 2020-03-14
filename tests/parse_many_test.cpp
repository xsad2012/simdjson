#include <cstring>
#ifndef _MSC_VER
#include <dirent.h>
#include <unistd.h>
#else
// Microsoft can't be bothered to provide standard utils.
#include <dirent_portable.h>
#endif
#include <cinttypes>

#include <cstdio>
#include <cstdlib>

#include "simdjson.h"

/**
 * Does the file filename end with the given extension.
 */
static bool has_extension(const char *filename, const char *extension) {
    const char *ext = strrchr(filename, '.');
    return ((ext != nullptr) && (strcmp(ext, extension) == 0));
}

bool starts_with(const char *pre, const char *str) {
    size_t len_pre = strlen(pre), len_str = strlen(str);
    return len_str < len_pre ? false : strncmp(pre, str, len_pre) == 0;
}

bool contains(const char *pre, const char *str) {
    return (strstr(str, pre) != nullptr);
}

bool validate(const char *dirname) {
    bool everything_fine = true;
    const char *extension1 = ".ndjson";
    const char *extension2 = ".jsonl";
    const char *extension3 = ".json"; // bad json files shoud fail

    size_t dirlen = strlen(dirname);
    struct dirent **entry_list;
    int c = scandir(dirname, &entry_list, nullptr, alphasort);
    if (c < 0) {
        fprintf(stderr, "error accessing %s \n", dirname);
        return false;
    }
    if (c == 0) {
        printf("nothing in dir %s \n", dirname);
        return false;
    }
    bool *is_file_as_expected = new bool[c];
    for (int i = 0; i < c; i++) {
        is_file_as_expected[i] = true;
    }
    size_t how_many = 0;
    bool needsep = (strlen(dirname) > 1) && (dirname[strlen(dirname) - 1] != '/');


    /*For all files in the folder*/
    for (int i = 0; i < c; i++) {
        const char *name = entry_list[i]->d_name;
        if (has_extension(name, extension1) || has_extension(name, extension2) || has_extension(name, extension3)) {

            /*  Finding the file path  */
            printf("validating: file %s ", name);
            fflush(nullptr);
            size_t filelen = strlen(name);
            char *fullpath = static_cast<char *>(malloc(dirlen + filelen + 1 + 1));
            strcpy(fullpath, dirname);
            if (needsep) {
                fullpath[dirlen] = '/';
                strcpy(fullpath + dirlen + 1, name);
            } else {
                strcpy(fullpath + dirlen, name);
            }


            /* The actual test*/
            auto [json, error] = simdjson::padded_string::load(fullpath);
            if (!error) {
                simdjson::document::parser parser;

                ++how_many;
                for (auto result : parser.parse_many(json)) {
                    error = result.error();
                }
            }
            printf("%s\n", error ? "ok" : "invalid");
            /* Check if the file is supposed to pass or not.  Print the results */
            if (contains("EXCLUDE", name)) {
                // skipping
                how_many--;
            } else if (starts_with("pass", name) and (has_extension(extension1, name) or has_extension(extension2, name)) and error) {
                is_file_as_expected[i] = false;
                printf("warning: file %s should pass but it fails. Error is: %s\n",
                       name, error_message(error));
                printf("size of file in bytes: %zu \n", json.size());
                everything_fine = false;
            } else if ( starts_with("fail", name) and (not starts_with("fail10.json", name)) and !error) {
                is_file_as_expected[i] = false;
                printf("warning: file %s should fail but it passes.\n", name);
                printf("size of file in bytes: %zu \n", json.size());
                everything_fine = false;
            }
            free(fullpath);
        }
    }

    printf("%zu files checked.\n", how_many);
    if (everything_fine) {
        printf("All ok!\n");
    } else {
        fprintf(stderr,
                "There were problems! Consider reviewing the following files:\n");
        for (int i = 0; i < c; i++) {
            if (!is_file_as_expected[i]) {
                fprintf(stderr, "%s \n", entry_list[i]->d_name);
            }
        }
    }
    printf("Note that json stream expects sequences of objects and arrays, so otherwise valid json files can fail by design.\n");
    for (int i = 0; i < c; ++i) {
        free(entry_list[i]);
    }
    free(entry_list);
    delete[] is_file_as_expected;
    return everything_fine;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <directorywithjsonfiles>"
                  << std::endl;
#ifndef SIMDJSON_TEST_DATA_DIR
        std::cout
        << "We are going to assume you mean to use the 'jsonchecker' directory."
        << std::endl;
    return validate("jsonchecker/") ? EXIT_SUCCESS : EXIT_FAILURE;
#else
        std::cout << "We are going to assume you mean to use the '"
                  << SIMDJSON_TEST_DATA_DIR << "' directory." << std::endl;
        return validate(SIMDJSON_TEST_DATA_DIR) ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
    }
    return validate(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
