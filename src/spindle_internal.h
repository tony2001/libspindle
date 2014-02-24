/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* maximum number of threads allowed in a pool */
#define SPINDLE_MAX_IN_POOL 200
#define MAX_QUEUE_MEMORY_SIZE 65536

#ifdef SPINDLE_DEBUG
# define TP_DEBUG(pool, ...) etfprintf((pool)->created, stderr, __VA_ARGS__);
#else
# define TP_DEBUG(pool, ...)
#endif

