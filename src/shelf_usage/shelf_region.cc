#include <stddef.h>
#include <stdint.h>
#include <string>

#include <fcntl.h> // for O_RDWR
#include <assert.h> // for assert()
#include <boost/filesystem.hpp>

#include "nvmm/error_code.h"
#include "shelf_mgmt/shelf_file.h"

#include "nvmm/log.h"

#include "shelf_usage/shelf_region.h"

namespace nvmm {

ShelfRegion::ShelfRegion(std::string pathname)
    : is_open_{false}, shelf_{pathname}
{
}

ShelfRegion::~ShelfRegion()
{
    if (IsOpen() == true)
    {
        (void)Close();
    }
}

ErrorCode ShelfRegion::Create(size_t size)
{
    TRACE();
    assert(IsOpen() == false);
    assert(shelf_.Exist() == true);
    
    // allocate memory for the shelf
    return shelf_.Truncate(size);
}

ErrorCode ShelfRegion::Destroy()
{
    TRACE();
    assert(IsOpen() == false);
    // truncate the size of the shelf file to 0
    // leave the deletion of the file to the caller
    return shelf_.Truncate(0);
}

ErrorCode ShelfRegion::Verify()
{
    assert(IsOpen() == false);
    ErrorCode ret = NO_ERROR;

    // TODO: check size == N*8GB???    
    if (shelf_.Exist() == true)
    {
        ret = NO_ERROR;
    }
    else
    {
        ret = SHELF_FILE_NOT_FOUND;
    }

    return ret;
}

ErrorCode ShelfRegion::Open(int flags)
{
    TRACE();
    assert(IsOpen() == false);

    ErrorCode ret = NO_ERROR;
    ret = Verify();    
    if (ret != NO_ERROR)
    {
        return ret;
    }

    // open the shelf
    ret = shelf_.Open(flags);
    if (ret != NO_ERROR)
    {
        return ret;
    }

    is_open_ = true;
    return NO_ERROR;
}

ErrorCode ShelfRegion::Close()
{
    TRACE();
    assert(IsOpen() == true);

    ErrorCode ret = NO_ERROR;
    // close the shelf
    ret = shelf_.Close();
    if (ret != NO_ERROR)
    {
        return ret;
    }
        
    is_open_ = false;
    return NO_ERROR;
}

size_t ShelfRegion::Size()
{
    assert(IsOpen() == true);
    return shelf_.Size();
}
 

ErrorCode ShelfRegion::Map(void *addr_hint, size_t length, int prot, int flags, loff_t offset, void **mapped_addr)
{
    TRACE();
    assert(IsOpen() == true);
    return shelf_.Map(addr_hint, length, prot, flags, offset, mapped_addr, true);
}

ErrorCode ShelfRegion::Unmap(void *mapped_addr, size_t length)
{
    TRACE();
    assert(IsOpen() == true);
    return shelf_.Unmap(mapped_addr, length, true);
}

} // namespace nvmm