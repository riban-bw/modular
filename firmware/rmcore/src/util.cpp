/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Utility helper implementation.
*/

#include "util.h"
#include <cstdint> // Provides fixed width integer types
#include <stdarg.h> // Provides varg
#include <cstdio> // Provides fprintf

uint8_t g_verbose = VERBOSE_INFO;

void debug(const char *format, ...) {
    if (g_verbose >= VERBOSE_DEBUG) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
  
void info(const char *format, ...) {
    if (g_verbose >= VERBOSE_INFO) {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }
}
  
void error(const char *format, ...) {
    if (g_verbose >= VERBOSE_ERROR) {
        va_list args;
        va_start(args, format);
        fprintf(stderr, "ERROR: ");
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
