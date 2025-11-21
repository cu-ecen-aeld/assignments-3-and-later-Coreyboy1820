/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    struct aesd_buffer_entry *currentEntry = {0};
    unsigned int retval = 0;
    unsigned int i = 0;

    while(i < buffer->length)
    {
        retval = aesd_circular_buffer_peek(buffer, &currentEntry, i);

        if(retval != success)
        {
            break;
        }
        
        if(char_offset < currentEntry->size)
        {
            *entry_offset_byte_rtn = char_offset;
            return currentEntry;
        }
        else
        {
            char_offset -= currentEntry->size;
        }

        i++;
    }

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
* @returns a pointer which needs to be freed when the buffer is full, NULL if nothing needs to be freed
*/
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    // normal case
    const char * retPtr = buffer->entry[buffer->in_offs].buffptr;
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs++;

    // handle wrap around
    if(buffer->in_offs >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        buffer->in_offs = 0;
    }

    if(buffer->full == true)
    {
        buffer->out_offs = buffer->in_offs;
    }
    else
    {
        buffer->length++;
        retPtr = NULL;
    }

    // case where buffer is full set buffer full to true
    if(buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }

    return retPtr;
}

extern inline int aesd_circular_buffer_read(struct aesd_circular_buffer *buffer, struct aesd_buffer_entry **readEntry)
{
    int bufferPeakRetVal = aesd_circular_buffer_peek(buffer, readEntry, 0);

    // if the buffer is full, set it to not full
    if(buffer->full)
    {
        buffer->full = false;
    }

    if(bufferPeakRetVal == success)
    {
        buffer->out_offs++;
        buffer->length--;
    }

    return bufferPeakRetVal;
}

extern int aesd_circular_buffer_peek(struct aesd_circular_buffer *buffer, struct aesd_buffer_entry **readEntry, unsigned int position)
{
    
    // If position indexs past the size of the buffer, set it back down to the start of the buffer
    unsigned int currentPosition = (buffer->out_offs + position) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // if the buffer is not empty, read the value back
    if(buffer->length == 0)
    {
        return buffer_empty;
    }
    else if(buffer->length < position)
    {
        return position_out_of_bounds;
    }
    else
    {
        *readEntry = &buffer->entry[currentPosition];
    }

    return success;
}

// extern void aesd_circular_buffer_print(struct aesd_circular_buffer *buffer)
// {
//     for(unsigned int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
//     {
//         printf("%d: %s\n", i, buffer->entry[i].buffptr); 
//     }
// }

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
