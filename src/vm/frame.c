#include "vm/frame.h"
#include <stdbool.h>
#include <stddef.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

/* Frame table. */
static struct list frame_table;

/* Initializes the frame table. */
void
frame_init (void)
{
  list_init (&frame_table);
}

/* Allocates a frame. */
void *
frame_alloc (void *upage, enum palloc_flags flags)
{
  struct frame *frame;
  void *page = palloc_get_page (PAL_USER | flags);

  if (page == NULL)
    page = frame_evict (flags);

  if (page != NULL)
    {
      frame = (struct frame *) malloc (sizeof (struct frame));
      frame->pagedir = thread_current ()->pagedir;
      frame->addr = page;
      frame->upage = upage;
      list_push_back (&frame_table, &frame->elem);
    }

  return page;
}

/* Frees a frame. */
void
frame_free (void *page)
{
  struct list_elem *e;
  struct frame *frame;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);
      if (frame->addr == page)
        {
          list_remove (e);
          palloc_free_page (frame->addr);
          free (frame);
          break;
        }
    }
}

/* Evicts a frame and return a address of new allocated frame. */
void *
frame_evict (enum palloc_flags flags)
{
  struct list_elem *e;
  struct frame *frame = NULL;
  struct page *page;

  /* Second chance algorithm. */
  e = list_begin (&frame_table);
  while (true)
    {
      frame = list_entry (e, struct frame, elem);
      if (pagedir_is_accessed (frame->pagedir, frame->upage))
        pagedir_set_accessed (frame->pagedir, frame->upage, false);
      else
        {
          if (pagedir_is_dirty (frame->pagedir, frame->upage))
            {
              page = page_find (frame->upage);
              page->valid = false;
              page->swap_idx = swap_out (frame->addr);
            }
          list_remove (e);
          pagedir_clear_page (frame->pagedir, frame->upage);
          palloc_free_page (frame->addr);
          free (frame);

          return palloc_get_page (PAL_USER | flags);
        }

      e = list_next (e);
      if (e == list_end (&frame_table))
        e = list_begin (&frame_table);
    }
}