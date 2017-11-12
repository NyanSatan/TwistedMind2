/* 
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


#define RESIZE_PARTITION 0x80086802
#define DKIOCGETBLOCKSIZE                     _IOR('d', 24, uint32_t)

uint64_t hfs_resize(uint64_t size, const char* path, uint32_t blocksize) {

	uint64_t required_size = size;

	uint64_t mod;
	if ( (mod = required_size % blocksize) != 0 ) {
		printf("Required size has to be multiple of blocksize (%u)\n", blocksize);
		required_size = required_size - mod + (uint64_t) blocksize;
		printf("Adjusting size to %llu to match next block\n", required_size);
	}

	
	printf("Resizing %s to %llu bytes\n", path, required_size);
	
	int err;
	if ( ( err = fsctl(path, RESIZE_PARTITION, &required_size, 0)) != 0) {
		printf("HFS resize failed. errno=%i\n", err);
        return 0;
	};

	return required_size;
}
