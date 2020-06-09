/* 
 *    Programmed By: Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
 *    Copyright 2016, 2017, 2018, 2019, 2020 (c)
 * 
 *    file: debug.h
 *    This file is part of the Layla Shell project.
 *
 *    Layla Shell is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Layla Shell is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Layla Shell.  If not, see <http://www.gnu.org/licenses/>.
 */    

#ifndef DEBUG_H
#define DEBUG_H

/* if you don't want to define the debug function, comment the following line */
#define DEBUG_MODE

/*
 * Depending on whether you commented the line above or not, the debug function
 * will either be defined as a null macro, or as a function that outputs debug 
 * messages.
 */
#ifndef DEBUG_MODE

#define debug(...)

#else

void __debug(const char *file, const char *function, char *format, ...);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define debug(...)  __debug(__FILENAME__, __FUNCTION__, __VA_ARGS__)

#endif  /* DEBUG_MODE */

#endif  /* DEBUG_H */
