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
#include <linux/slab.h>
#define FREE kfree
#else
#include <string.h>
#include <stdlib.h>
#define FREE free
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
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer, size_t char_offset, size_t *entry_offset_byte_rtn )
{
    if ((buffer == NULL) || (entry_offset_byte_rtn == NULL))
        return NULL;

    size_t new_index = 0;
    uint32_t i = 0;
    size_t curr_size = 0;

    struct aesd_buffer_entry *entry;
    for(i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        new_index = ((buffer->out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
        entry = &(buffer->entry[new_index]);

        if (char_offset < (curr_size + entry->size)) 
        {
            *entry_offset_byte_rtn = char_offset - curr_size;
            return entry;
        }
        curr_size += entry->size; 
    } 
    return NULL;
}


/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char * aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{    
    const char* ret_entry = NULL;

    if(buffer == NULL || add_entry == NULL || add_entry->size == 0 || add_entry->buffptr == NULL) 
        return ret_entry;

    if(buffer->full)
        ret_entry = buffer->entry[buffer->out_offs].buffptr;

    buffer->entry[buffer->in_offs] = *add_entry;
    if ((buffer->in_offs == buffer->out_offs) && buffer->full)
    {
        buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        buffer->out_offs = buffer->in_offs; 
    } 
    else 
    {
        buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        if(buffer->in_offs == buffer->out_offs)

            buffer->full = true;
        else 
            buffer->full = false;   
    }
    return ret_entry;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer) 
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
    buffer->full = false; 
}

void aesd_circular_buffer_deallocate(struct aesd_circular_buffer *buffer) 
{
    struct aesd_circular_buffer *buf = buffer;
    struct aesd_buffer_entry *entry;
    uint8_t index;

    AESD_CIRCULAR_BUFFER_FOREACH(entry,buf,index) 
    {
        if(entry->buffptr != NULL)
        {
            FREE((void*)entry->buffptr);       
        }
    }
}
