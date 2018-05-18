/*
 *  (c) Copyright 2016-2017 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the
 *  Application containing code generated by the Library and added to the
 *  Application during this compilation process under terms of your choice,
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

/*
  modified from the stack implementation from Mark
 */

#include <assert.h>

#include "nvmm/global_ptr.h"
#include "shelf_usage/smart_shelf.h"

#include "nvmm/fam.h"

#include "shelf_usage/zone_entry_stack.h"
#include "shelf_usage/zone_entry.h"

namespace nvmm {

void ZoneEntryStack::push(void *addr, uint64_t idx_) {
    // idx 0 is reserved for empty stack so all actual idxes are shifted right by 1
    uint64_t idx = idx_+1;

    uint64_t* entry_ptr = (uint64_t*)addr + idx;
    // TODO: can we avoid this atomic read? maybe we should pass in the zone_entry value?
    zone_entry entry = (zone_entry)fam_atomic_u64_read(entry_ptr);

    uint64_t old[2], store[2], result[2];
    // would non-atomic reads be faster here?
    fam_atomic_u128_read(&head, old);
    for (;;) {
        entry.link_next(old[0]);
        // would an atomic write be faster here?
        fam_atomic_u64_write(entry_ptr, (uint64_t)entry);

        store[0] = idx;
        store[1] = old[1] + 1;
        fam_atomic_u128_compare_and_store(&head, old, store, result);
        if (result[0]==old[0] && result[1]==old[1])
            return;

        old[0] = result[0];
        old[1] = result[1];
    }
}

uint64_t ZoneEntryStack::pop(void *addr) {
    uint64_t old[2], store[2], result[2];
    // would non-atomic reads be faster here?
    fam_atomic_u128_read(&head, old);
    for (;;) {
        uint64_t idx = old[0];
        // we reserve idx 0 to indicate if the stack is empty or not
        if (idx == 0)
            break;

        uint64_t* entry_ptr = (uint64_t*)addr + idx;
        // would an atomic read be faster here?
        zone_entry entry = (zone_entry)fam_atomic_u64_read(entry_ptr);

        store[0] = entry.next();
        store[1] = old[1] + 1;
        fam_atomic_u128_compare_and_store(&head, old, store, result);
        if (result[0]==old[0] && result[1]==old[1])
            return idx-1;

        old[0] = result[0];
        old[1] = result[1];
    }

    return 0;
}


}