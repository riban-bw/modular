/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Utility helper functions header.
*/

#pragma once

enum VERBOSE {
    VERBOSE_SILENT  = 0,
    VERBOSE_ERROR   = 1,
    VERBOSE_INFO    = 2,
    VERBOSE_DEBUG   = 3
};

void debug(const char *format, ...);
  
void info(const char *format, ...);

void error(const char *format, ...);
