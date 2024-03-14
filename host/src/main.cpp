/*  riban modular - copyright riban ltd 2024
    Copyright 2023 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Core host application
*/

#include <cstdio> // Provides printf
#include <getopt.h>

const char* version_str = "0.0";
bool g_debug = false;

void print_version() {
    printf("riban modular v%s Copyright riban ltd 2024\n", version_str);
}

void print_help() {
    print_version();
    printf("Usage: rmcore <options>\n");
    printf("\tv --version\tShow version\n");
    printf("\td --debug\tEnable debug output\n");
    printf("\th --help\tShow this help\n");
}

bool parse_cmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"debug", no_argument, 0, 'd'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvd", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd': g_debug = true; break;
            case 'h': print_help(); break;
            case 'v': print_version(); break;
            case '?': return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if(parse_cmdline(argc, argv)) {
        // Error parsing command line
        return -1;
    }
    //!@todo Implement app
}